#include <skutils/unddos.h>

namespace skutils::unddos {

    origin_entry_setting::origin_entry_setting() {
        clear();
    }

    origin_entry_setting::origin_entry_setting(const origin_entry_setting &other) {
        assign(other);
    }

    origin_entry_setting::origin_entry_setting(origin_entry_setting &&other) {
        assign(other);
        other.clear();
    }

    origin_entry_setting::~origin_entry_setting() {
        clear();
    }

    origin_entry_setting &origin_entry_setting::operator=(const origin_entry_setting &other) {
        assign(other);
        return (*this);
    }

    void origin_entry_setting::load_defaults_for_any_origin() {
        load_friendly_for_any_origin();
        // load_reasonable_for_any_origin();
    }

    void origin_entry_setting::load_friendly_for_any_origin() {
        clear();
        m_originWildcards.push_back("*");
        m_defaultMaxCallsPerSec = 500;
        m_defaultMaxCallsPerMin = 15000;
        m_banPerSecDuration = duration(15);
        m_banPerMinDuration = duration(120);
        m_maxWSConn = 50;
        load_recommended_custom_methods_as_multiplier_of_default();
    }


    void origin_entry_setting::load_unlim_for_any_origin() {
        clear();
        m_originWildcards.push_back("*");
        m_defaultMaxCallsPerSec = std::numeric_limits<size_t>::max();
        m_defaultMaxCallsPerMin = std::numeric_limits<size_t>::max();
        m_banPerSecDuration = duration(0);
        m_banPerMinDuration = duration(0);
        m_maxWSConn = std::numeric_limits<size_t>::max();
        load_recommended_custom_methods_as_multiplier_of_default();
    }

    void origin_entry_setting::load_unlim_for_localhost_only() {
        clear();
        m_originWildcards.push_back("127.0.0.*");
        m_originWildcards.push_back("::1");
        m_defaultMaxCallsPerSec = std::numeric_limits<size_t>::max();
        m_defaultMaxCallsPerMin = std::numeric_limits<size_t>::max();
        m_banPerSecDuration = duration(0);
        m_banPerMinDuration = duration(0);
        m_maxWSConn = std::numeric_limits<size_t>::max();
        load_recommended_custom_methods_as_multiplier_of_default();
    }

    void origin_entry_setting::load_custom_method_as_multiplier_of_default(
            const char *strMethod, double lfMultiplier) {
        if (strMethod == nullptr || strMethod[0] == '\0' || lfMultiplier <= 0.0)
            return;
        custom_method_setting cme;
        cme.max_calls_per_second_ = size_t(m_defaultMaxCallsPerSec * lfMultiplier);
        cme.max_calls_per_minute_ = size_t(m_defaultMaxCallsPerMin * lfMultiplier);
        m_mapCustomMethodSettings[strMethod] = cme;
    }

    void origin_entry_setting::load_recommended_custom_methods_as_multiplier_of_default(
            double lfMultiplier) {
        static const char *g_arr[] = {"web3_clientVersion", "web3_sha3", "net_version", "eth_syncing",
                                      "eth_protocolVersion", "eth_gasPrice", "eth_blockNumber", "eth_getBalance",
                                      "eth_getBlockByHash", "eth_getBlockByNumber", "eth_getTransactionCount",
                                      "eth_getTransactionReceipt", "eth_getTransactionByHash",
                                      "eth_getTransactionByBlockHashAndIndex",
                                      "eth_getTransactionByBlockNumberAndIndex"};
        for (size_t i = 0; i < sizeof(g_arr) / sizeof(g_arr[0]); ++i)
            load_custom_method_as_multiplier_of_default(g_arr[i], lfMultiplier);
    }


    bool origin_entry_setting::empty() const {
        if (!m_originWildcards.empty())
            return false;
        return true;
    }

    void origin_entry_setting::clear() {
        m_originWildcards.clear();
        m_defaultMaxCallsPerSec = 0;
        m_defaultMaxCallsPerMin = 0;
        m_banPerSecDuration = duration(0);
        m_banPerMinDuration = duration(0);
        m_maxWSConn = 0;
        m_mapCustomMethodSettings.clear();
    }

    origin_entry_setting &origin_entry_setting::assign(const origin_entry_setting &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        clear();
        m_originWildcards = other.m_originWildcards;
        m_defaultMaxCallsPerSec = other.m_defaultMaxCallsPerSec;
        m_defaultMaxCallsPerMin = other.m_defaultMaxCallsPerMin;
        m_banPerSecDuration = other.m_banPerSecDuration;
        m_banPerMinDuration = other.m_banPerMinDuration;
        m_maxWSConn = other.m_maxWSConn;
        m_mapCustomMethodSettings = other.m_mapCustomMethodSettings;
        return (*this);
    }

    origin_entry_setting &origin_entry_setting::merge(const origin_entry_setting &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        if (m_originWildcards != other.m_originWildcards)
            return (*this);
        m_defaultMaxCallsPerSec = std::min(m_defaultMaxCallsPerSec, other.m_defaultMaxCallsPerSec);
        m_defaultMaxCallsPerMin = std::min(m_defaultMaxCallsPerMin, other.m_defaultMaxCallsPerMin);
        m_banPerSecDuration = std::max(m_banPerSecDuration, other.m_banPerSecDuration);
        m_banPerMinDuration = std::max(m_banPerMinDuration, other.m_banPerMinDuration);
        m_maxWSConn = std::min(m_maxWSConn, other.m_maxWSConn);
        if (!other.m_mapCustomMethodSettings.empty()) {
            nlohmann::json joCMS = nlohmann::json::object();
            map_custom_method_settings_t::const_iterator itWalk =
                    other.m_mapCustomMethodSettings.cbegin(),
                    itEnd =
                    other.m_mapCustomMethodSettings.cend();
            for (; itWalk != itEnd; ++itWalk) {
                const custom_method_setting &cme = itWalk->second;
                map_custom_method_settings_t::iterator itFind =
                        m_mapCustomMethodSettings.find(itWalk->first);
                if (itFind != m_mapCustomMethodSettings.end()) {
                    itFind->second.merge(cme);  // merge with existing
                    continue;
                }
                m_mapCustomMethodSettings[itWalk->first] = cme;  // add mew
            }
        }
        return (*this);
    }

    void origin_entry_setting::fromJSON(const nlohmann::json &jo) {
        clear();
        if (jo.find("origin") != jo.end()) {
            nlohmann::json jarrWildcards = jo["origin"];
            if (jarrWildcards.is_string())
                m_originWildcards.push_back(jarrWildcards.get<std::string>());
            else if (jarrWildcards.is_array()) {
                for (const nlohmann::json &joWildcard: jarrWildcards) {
                    if (joWildcard.is_string())
                        m_originWildcards.push_back(joWildcard.get<std::string>());
                }
            }
        }
        if (jo.find("max_calls_per_second") != jo.end())
            m_defaultMaxCallsPerSec = jo["max_calls_per_second"].get<size_t>();
        if (jo.find("max_calls_per_minute") != jo.end())
            m_defaultMaxCallsPerMin = jo["max_calls_per_minute"].get<size_t>();
        if (jo.find("ban_peak") != jo.end())
            m_banPerSecDuration = jo["ban_peak"].get<size_t>();
        if (jo.find("ban_lengthy") != jo.end())
            m_banPerMinDuration = jo["ban_lengthy"].get<size_t>();
        if (jo.find("max_ws_conn") != jo.end())
            m_maxWSConn = jo["max_ws_conn"].get<size_t>();
        if (jo.find("custom_method_settings") != jo.end()) {
            const nlohmann::json &joCMS = jo["custom_method_settings"];
            for (auto it = joCMS.cbegin(); it != joCMS.cend(); ++it) {
                const nlohmann::json &joMethod = it.value();
                custom_method_setting cme;
                if (joMethod.find("max_calls_per_second") != jo.end())
                    cme.max_calls_per_second_ = joMethod["max_calls_per_second"].get<size_t>();
                if (joMethod.find("max_calls_per_minute") != jo.end())
                    cme.max_calls_per_minute_ = joMethod["max_calls_per_minute"].get<size_t>();
                m_mapCustomMethodSettings[it.key()] = cme;
            }
        }
    }

    void origin_entry_setting::toJSON(nlohmann::json &jo) const {
        jo = nlohmann::json::object();
        nlohmann::json jarrWildcards = nlohmann::json::array();
        for (const std::string &wildcard: m_originWildcards)
            jarrWildcards.push_back(wildcard);
        jo["origin"] = jarrWildcards;
        jo["max_calls_per_second"] = m_defaultMaxCallsPerSec;
        jo["max_calls_per_minute"] = m_defaultMaxCallsPerMin;
        jo["ban_peak"] = m_banPerSecDuration;
        jo["ban_lengthy"] = m_banPerMinDuration;
        jo["max_ws_conn"] = m_maxWSConn;
        if (!m_mapCustomMethodSettings.empty()) {
            nlohmann::json joCMS = nlohmann::json::object();
            map_custom_method_settings_t::const_iterator itWalk = m_mapCustomMethodSettings.cbegin(),
                    itEnd = m_mapCustomMethodSettings.cend();
            for (; itWalk != itEnd; ++itWalk) {
                const custom_method_setting &cme = itWalk->second;
                nlohmann::json joMethod = nlohmann::json::object();
                joMethod["max_calls_per_second"] = cme.max_calls_per_second_;
                joMethod["max_calls_per_minute"] = cme.max_calls_per_minute_;
                joCMS[itWalk->first] = joMethod;
            }
            jo["custom_method_settings"] = joCMS;
        }
    }

    bool origin_entry_setting::match_origin(const char *origin) const {
        if (origin == nullptr || (*origin) == '\0')
            return false;
        for (const std::string &wildcard: m_originWildcards) {
            if (skutils::tools::wildcmp(wildcard.c_str(), origin))
                return true;
        }
        return false;
    }

    size_t origin_entry_setting::max_calls_per_second(const char *strMethod) const {
        if (strMethod == nullptr || strMethod[0] == '\0')
            return m_defaultMaxCallsPerSec;
        map_custom_method_settings_t::const_iterator itFind =
                m_mapCustomMethodSettings.find(strMethod),
                itEnd = m_mapCustomMethodSettings.cend();
        if (itFind == itEnd)
            return m_defaultMaxCallsPerSec;
        const custom_method_setting &cme = itFind->second;
        const size_t cnt = std::max(m_defaultMaxCallsPerSec, cme.max_calls_per_second_);
        return cnt;
    }

    size_t origin_entry_setting::max_calls_per_minute(const char *strMethod) const {
        if (strMethod == nullptr || strMethod[0] == '\0')
            return m_defaultMaxCallsPerMin;
        map_custom_method_settings_t::const_iterator itFind =
                m_mapCustomMethodSettings.find(strMethod),
                itEnd = m_mapCustomMethodSettings.cend();
        if (itFind == itEnd)
            return m_defaultMaxCallsPerMin;
        const custom_method_setting &cme = itFind->second;
        const size_t cnt = std::max(m_defaultMaxCallsPerMin, cme.max_calls_per_minute_);
        return cnt;
    }


    settings::settings() {
        clear();
    }

    settings::settings(const settings &other) {
        assign(other);
    }

    settings::settings(settings &&other) {
        assign(other);
        other.clear();
    }

    settings::~settings() {
        clear();
    }

    settings &settings::operator=(const settings &other) {
        assign(other);
        return (*this);
    }

    bool settings::empty() const {
        if (!m_enabled)
            return true;
        if (!m_origins.empty())
            return false;
        if (!m_globalLimit.empty())
            return false;
        return true;
    }

    void settings::clear() {
        m_enabled = true;
        m_origins.clear();
        m_globalLimit.clear();
    }

    settings &settings::assign(const settings &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        clear();
        m_enabled = other.m_enabled;
        m_origins = other.m_origins;
        m_globalLimit = other.m_globalLimit;
        return (*this);
    }

    settings &settings::merge(const settings &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        for (const origin_entry_setting &oe: other.m_origins)
            merge(oe);
        m_globalLimit.merge(other.m_globalLimit);
        return (*this);
    }

    settings &settings::merge(const origin_entry_setting &oe) {
        size_t i = indexOfOrigin(oe);
        if (i == std::string::npos)
            m_origins.push_back(oe);
        else
            m_origins[i].merge(oe);
        return (*this);
    }

    size_t settings::indexOfOrigin(const origin_entry_setting &oe, size_t idxStart) {
        for (const std::string &wildcard: oe.m_originWildcards) {
            size_t i = indexOfOrigin(wildcard, idxStart);
            if (i != std::string::npos)
                return i;
        }
        return std::string::npos;
    }

    size_t settings::indexOfOrigin(const char *origin_wildcard, size_t idxStart) {
        if (origin_wildcard == nullptr || (*origin_wildcard) == '\0')
            return std::string::npos;
        size_t cnt = m_origins.size();
        size_t i = (idxStart == std::string::npos) ? 0 : (idxStart + 1);
        for (; i < cnt; ++i) {
            const origin_entry_setting &oe = m_origins[i];
            for (const std::string &wildcard: oe.m_originWildcards) {
                if (wildcard == origin_wildcard)
                    return i;
            }
        }
        return std::string::npos;
    }

    size_t settings::indexOfOrigin(const std::string &origin_wildcard, size_t idxStart) {
        if (origin_wildcard.empty())
            return std::string::npos;
        return indexOfOrigin(origin_wildcard.c_str(), idxStart);
    }

    void settings::fromJSON(const nlohmann::json &jo) {
        clear();
        if (jo.find("origins") != jo.end()) {
            const nlohmann::json &joOrigins = jo["origins"];
            if (joOrigins.is_array()) {
                for (const nlohmann::json &joOrigin: joOrigins) {
                    origin_entry_setting oe;
                    oe.fromJSON(joOrigin);
                    m_origins.push_back(oe);
                }
            }
        }
        if (jo.find("global") != jo.end()) {
            const nlohmann::json &joGlobalLimit = jo["global"];
            origin_entry_setting oe;
            oe.fromJSON(joGlobalLimit);
            m_globalLimit = oe;
        } else
            m_globalLimit.load_unlim_for_any_origin();
        bool isEnabled = true;
        if (jo.find("enabled") != jo.end()) {
            const nlohmann::json &joEnabled = jo["enabled"];
            if (joEnabled.is_boolean())
                isEnabled = joEnabled.get<bool>();
        }
        m_enabled = isEnabled;
    }

    void settings::toJSON(nlohmann::json &jo) const {
        jo = nlohmann::json::object();
        nlohmann::json joOrigins = nlohmann::json::array();
        for (const origin_entry_setting &oe: m_origins) {
            nlohmann::json joOrigin = nlohmann::json::object();
            oe.toJSON(joOrigin);
            joOrigins.push_back(joOrigin);
        }
        nlohmann::json joGlobalLimit = nlohmann::json::object();
        m_globalLimit.toJSON(joGlobalLimit);
        jo["enabled"] = m_enabled;
        jo["origins"] = joOrigins;
        jo["global"] = joGlobalLimit;
    }

    size_t settings::find_origin_entry_setting_match(const char *origin, size_t idxStart) const {
        if (origin == nullptr || (*origin) == '\0')
            return std::string::npos;
        size_t cnt = m_origins.size();
        size_t i = (idxStart == std::string::npos) ? 0 : (idxStart + 1);
        for (; i < cnt; ++i) {
            const origin_entry_setting &oe = m_origins[i];
            if (oe.match_origin(origin))
                return i;
        }
        return std::string::npos;
    }

    origin_entry_setting &settings::find_origin_entry_setting(const char *origin) {
        size_t i = find_origin_entry_setting_match(origin);
        if (i != std::string::npos)
            return m_origins[i];
        return auto_append_any_origin_rule();
    }

    origin_entry_setting &settings::auto_append_any_origin_rule() {
        if (!m_origins.empty()) {
            size_t i = find_origin_entry_setting_match("*");
            if (i != std::string::npos)
                return m_origins[i];
        }
        origin_entry_setting oe;
        oe.load_defaults_for_any_origin();
        m_origins.push_back(oe);
        return m_origins[m_origins.size() - 1];
    }

    time_entry::time_entry(time_tick_mark ttm) : ttm_(ttm) {}

    time_entry::time_entry(const time_entry &other) {
        assign(other);
    }

    time_entry::time_entry(time_entry &&other) {
        assign(other);
        other.clear();
    }

    time_entry::~time_entry() {
        clear();
    }

    time_entry &time_entry::operator=(const time_entry &other) {
        return assign(other);
    }

    bool time_entry::empty() const {
        if (ttm_ != time_tick_mark(0))
            return false;
        return true;
    }

    void time_entry::clear() {
        ttm_ = time_tick_mark(0);
    }

    time_entry &time_entry::assign(const time_entry &other) {
        clear();
        ttm_ = other.ttm_;
        return (*this);
    }

    int time_entry::compare(const time_entry &other) const {
        if (ttm_ < other.ttm_)
            return -1;
        if (ttm_ > other.ttm_)
            return 1;
        return 0;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    tracked_origin::tracked_origin(const char *origin, time_tick_mark ttm)
            : m_origin((origin != nullptr && origin[0] != '\0') ? origin : "") {
        recordUse(ttm);
    }

    tracked_origin::tracked_origin(const std::string &origin, time_tick_mark ttm)
            : m_origin(origin) {
        recordUse(ttm);
    }

    tracked_origin::tracked_origin(const tracked_origin &other) {
        assign(other);
    }

    tracked_origin::tracked_origin(tracked_origin &&other) {
        assign(other);
        other.clear();
    }

    tracked_origin::~tracked_origin() {
        clear();
    }

    tracked_origin &tracked_origin::operator=(const tracked_origin &other) {
        return assign(other);
    }

    bool tracked_origin::empty() const {
        if (!m_origin.empty())
            return false;
        if (!(m_currentSecondUseCounter == 0 && m_currentMinuteUseCounter == 0))
            return false;
        return true;
    }

    void tracked_origin::clear() {
        m_origin.clear();
        m_currentSecondUseCounter = 0;
        m_currentMinuteUseCounter = 0;
        clear_ban();
    }

    tracked_origin &tracked_origin::assign(const tracked_origin &other) {
        clear();
        m_origin = other.m_origin;
        m_currentSecondUseCounter.store(other.m_currentSecondUseCounter);
        m_currentMinuteUseCounter.store(other.m_currentMinuteUseCounter);
        m_banUntil = other.m_banUntil;
        return (*this);
    }

    int tracked_origin::compare(const tracked_origin &other) const {
        int n = m_origin.compare(other.m_origin);
        return n;
    }

    int tracked_origin::compare(const char *origin) const {
        int n = m_origin.compare(origin ? origin : "");
        return n;
    }

    int tracked_origin::compare(const std::string &origin) const {
        int n = m_origin.compare(origin);
        return n;
    }

    void tracked_origin::clear_ban() {
        m_banUntil = time_tick_mark(0);
    }

    bool tracked_origin::isBanned(time_tick_mark _time) {
        return (_time <= m_banUntil);
    }

    void tracked_origin::recordUse(time_tick_mark _useTimeSec) {
        static constexpr uint64_t SECONDS_IN_MINUTE = 60;
        static constexpr uint64_t SECONDS_IN_HOUR = 60 * 60;

        auto minute = _useTimeSec / SECONDS_IN_MINUTE;
        auto hour = _useTimeSec / SECONDS_IN_MINUTE;

        if (minute > m_currentMinute) {
            // next minute arrived. Reset use counter
            m_currentMinuteUseCounter = 0;
            m_currentMinute = minute;
        }

        if (hour > m_currentSecond) {
            // next hour arrived. Reset use counter
            m_currentSecondUseCounter = 0;
            m_currentSecond = hour;
        }

        m_currentSecondUseCounter++;
        m_currentSecondUseCounter++;

    }

    algorithm::algorithm() {}

    algorithm::algorithm(const settings &st) {
        m_settings = st;
    }

    algorithm::~algorithm() {}

    algorithm &algorithm::operator=(const settings &st) {
        lock_type lock(x_mtx);
        m_settings = st;
        return (*this);
    }

    e_high_load_detection_result_t algorithm::register_call_from_origin(
            const char *_origin, const char *_strMethod, time_tick_mark _callTime, duration _durationToPast) {
        if (!m_settings.m_enabled)
            return e_high_load_detection_result_t::ehldr_no_error;
        if (_origin == nullptr || _origin[0] == '\0')
            return e_high_load_detection_result_t::ehldr_bad_origin;
        adjust_now_tick_mark(_callTime);
        lock_type lock(x_mtx);
        //
        m_globalOrigin.recordUse(time_entry(_callTime));
        if (m_globalOrigin.isBanned(_callTime))
            return e_high_load_detection_result_t::ehldr_already_banned;  // still banned

        tracked_origins_t::iterator itFind = m_trackedOriginsMap.find(_origin),
                itEnd = m_trackedOriginsMap.end();
        if (itFind == itEnd) {
            tracked_origin to(_origin, _callTime);
            m_trackedOriginsMap.insert(to);
            return e_high_load_detection_result_t::ehldr_no_error;
        }

        tracked_origin &to = const_cast< tracked_origin & >( *itFind );
        to.recordUse(time_entry(_callTime));
        if (to.isBanned(_callTime))
            return e_high_load_detection_result_t::ehldr_already_banned;  // still banned
        const origin_entry_setting &oe = m_settings.find_origin_entry_setting(_origin);
        auto maxCallsPerMinute = oe.max_calls_per_minute(_strMethod);
        if (maxCallsPerMinute > 0) {
            if (to.m_currentMinuteUseCounter > maxCallsPerMinute) {
                to.m_banUntil = _callTime + oe.m_banPerMinDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_min;  // ban by too high load per min
            }
        }
        auto maxCallsPerSecond = oe.max_calls_per_second(_strMethod);
        if (maxCallsPerSecond > 0) {
            if (to.m_currentSecondUseCounter > maxCallsPerSecond) {
                to.m_banUntil = _callTime + oe.m_banPerSecDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_sec;
            }
        }

        maxCallsPerMinute = m_settings.m_globalLimit.max_calls_per_minute(_strMethod);
        if (maxCallsPerMinute > 0) {
            if (m_globalOrigin.m_currentMinuteUseCounter > maxCallsPerMinute) {
                m_globalOrigin.m_banUntil = _callTime + m_settings.m_globalLimit.m_banPerMinDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_min;
            }
        }
        maxCallsPerSecond = m_settings.m_globalLimit.max_calls_per_second(_strMethod);
        if (maxCallsPerSecond > 0) {
            if (m_globalOrigin.m_currentSecondUseCounter > maxCallsPerMinute) {
                m_globalOrigin.m_banUntil = _callTime + m_settings.m_globalLimit.m_banPerSecDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_sec;  // ban by too high load per second
            }
        }

        return e_high_load_detection_result_t::ehldr_no_error;
    }

    bool algorithm::is_ban_ws_conn_for_origin(const char *origin) const {
        if (!m_settings.m_enabled)
            return false;
        if (origin == nullptr || origin[0] == '\0')
            return true;
        lock_type lock(x_mtx);
        if (m_WsConnCountGlobal > m_settings.m_globalLimit.m_maxWSConn)
            return true;
        map_ws_conn_counts_t::const_iterator itFind = m_mapWsConnCounts.find(origin),
                itEnd = m_mapWsConnCounts.end();
        if (itFind == itEnd)
            return false;
        const origin_entry_setting &oe = m_settings.find_origin_entry_setting(origin);
        if (itFind->second > oe.m_maxWSConn)
            return true;
        return false;
    }

    e_high_load_detection_result_t algorithm::register_ws_conn_for_origin(const char *origin) {
        if (!m_settings.m_enabled)
            return e_high_load_detection_result_t::ehldr_no_error;
        if (origin == nullptr || origin[0] == '\0')
            return e_high_load_detection_result_t::ehldr_bad_origin;
        lock_type lock(x_mtx);
        ++m_WsConnCountGlobal;
        if (m_WsConnCountGlobal > m_settings.m_globalLimit.m_maxWSConn)
            return e_high_load_detection_result_t::ehldr_detected_ban_per_sec;
        map_ws_conn_counts_t::iterator itFind = m_mapWsConnCounts.find(origin),
                itEnd = m_mapWsConnCounts.end();
        if (itFind == itEnd) {
            m_mapWsConnCounts[origin] = 1;
            itFind = m_mapWsConnCounts.find(origin);
        } else
            ++itFind->second;
        const origin_entry_setting &oe = m_settings.find_origin_entry_setting(origin);
        if (itFind->second > oe.m_maxWSConn)
            return e_high_load_detection_result_t::ehldr_detected_ban_per_sec;
        return e_high_load_detection_result_t::ehldr_no_error;
    }

    bool algorithm::unregister_ws_conn_for_origin(const char *origin) {
        if (origin == nullptr || origin[0] == '\0') {
            if (!m_settings.m_enabled)
                return true;
            return false;
        }
        lock_type lock(x_mtx);
        if (m_WsConnCountGlobal > 0)
            --m_WsConnCountGlobal;
        map_ws_conn_counts_t::iterator itFind = m_mapWsConnCounts.find(origin),
                itEnd = m_mapWsConnCounts.end();
        if (itFind == itEnd) {
            if (!m_settings.m_enabled)
                return true;
            return false;
        }
        if (itFind->second >= 1)
            --itFind->second;
        if (itFind->second == 0)
            m_mapWsConnCounts.erase(itFind);
        return true;
    }

    bool algorithm::load_settings_from_json(const nlohmann::json &joUnDdosSettings) {
        lock_type lock(x_mtx);
        try {
            settings new_settings;
            new_settings.fromJSON(joUnDdosSettings);
            m_settings = new_settings;
            m_settings.auto_append_any_origin_rule();
            return true;
        } catch (...) {
            return false;
        }
    }

    settings algorithm::get_settings() const {
        lock_type lock(x_mtx);
        m_settings.auto_append_any_origin_rule();
        settings copied = m_settings;
        return copied;
    }

    void algorithm::set_settings(const settings &new_settings) const {
        lock_type lock(x_mtx);
        m_settings = new_settings;
        m_settings.auto_append_any_origin_rule();
    }

    nlohmann::json algorithm::get_settings_json() const {
        lock_type lock(x_mtx);
        m_settings.auto_append_any_origin_rule();
        nlohmann::json joUnDdosSettings = nlohmann::json::object();
        m_settings.toJSON(joUnDdosSettings);
        return joUnDdosSettings;
    }

};  // namespace skutils::unddos
