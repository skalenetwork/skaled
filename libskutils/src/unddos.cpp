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
        if (!m_originSettings.empty())
            return false;
        if (!m_globalLimitSetting.empty())
            return false;
        return true;
    }

    void settings::clear() {
        m_enabled = true;
        m_originSettings.clear();
        m_globalLimitSetting.clear();
    }

    settings &settings::assign(const settings &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        clear();
        m_enabled = other.m_enabled;
        m_originSettings = other.m_originSettings;
        m_globalLimitSetting = other.m_globalLimitSetting;
        return (*this);
    }

    settings &settings::merge(const settings &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        for (const origin_entry_setting &oe: other.m_originSettings)
            merge(oe);
        m_globalLimitSetting.merge(other.m_globalLimitSetting);
        return (*this);
    }

    settings &settings::merge(const origin_entry_setting &oe) {
        size_t i = indexOfOrigin(oe);
        if (i == std::string::npos)
            m_originSettings.push_back(oe);
        else
            m_originSettings[i].merge(oe);
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
        size_t cnt = m_originSettings.size();
        size_t i = (idxStart == std::string::npos) ? 0 : (idxStart + 1);
        for (; i < cnt; ++i) {
            const origin_entry_setting &oe = m_originSettings[i];
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
                    m_originSettings.push_back(oe);
                }
            }
        }
        if (jo.find("global") != jo.end()) {
            const nlohmann::json &joGlobalLimit = jo["global"];
            origin_entry_setting oe;
            oe.fromJSON(joGlobalLimit);
            m_globalLimitSetting = oe;
        } else
            m_globalLimitSetting.load_unlim_for_any_origin();
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
        for (const origin_entry_setting &oe: m_originSettings) {
            nlohmann::json joOrigin = nlohmann::json::object();
            oe.toJSON(joOrigin);
            joOrigins.push_back(joOrigin);
        }
        nlohmann::json joGlobalLimit = nlohmann::json::object();
        m_globalLimitSetting.toJSON(joGlobalLimit);
        jo["enabled"] = m_enabled;
        jo["origins"] = joOrigins;
        jo["global"] = joGlobalLimit;
    }

    size_t settings::find_origin_entry_setting_match(const char *origin, size_t idxStart) const {
        if (origin == nullptr || (*origin) == '\0')
            return std::string::npos;
        size_t cnt = m_originSettings.size();
        size_t i = (idxStart == std::string::npos) ? 0 : (idxStart + 1);
        for (; i < cnt; ++i) {
            const origin_entry_setting &oe = m_originSettings[i];
            if (oe.match_origin(origin))
                return i;
        }
        return std::string::npos;
    }

    origin_entry_setting &settings::find_origin_entry_setting(const char *origin) {
        size_t i = find_origin_entry_setting_match(origin);
        if (i != std::string::npos)
            return m_originSettings[i];
        return auto_append_any_origin_rule();
    }

    origin_entry_setting &settings::auto_append_any_origin_rule() {
        if (!m_originSettings.empty()) {
            size_t i = find_origin_entry_setting_match("*");
            if (i != std::string::npos)
                return m_originSettings[i];
        }
        origin_entry_setting oe;
        oe.load_defaults_for_any_origin();
        m_originSettings.push_back(oe);
        return m_originSettings[m_originSettings.size() - 1];
    }


    tracked_origin::tracked_origin(const char *origin, time_tick_mark ttm)
            : m_origin((origin != nullptr && origin[0] != '\0') ? origin : "") {
        recordUse(ttm, "");
    }

    tracked_origin::tracked_origin(const std::string &origin, time_tick_mark ttm)
            : m_origin(origin) {
        recordUse(ttm, "");
    }

    tracked_origin::~tracked_origin() {
    }


    void tracked_origin::clearBan() {
        m_banUntilSec = time_tick_mark(0);
    }

    bool tracked_origin::isBanned(uint64_t _timeSec) {
        return (_timeSec <= m_banUntilSec);
    }

    void tracked_origin::recordUse(uint64_t _useTimeSec, const char* _strMethod) {

        std::string method = (_strMethod? _strMethod : "");


        static constexpr uint64_t SECONDS_IN_MINUTE = 60;
        auto minute = _useTimeSec / SECONDS_IN_MINUTE;

        std::lock_guard lock(x_mutex);

        if ((uint64_t )_useTimeSec > m_currentSec) {
            // next hour arrived. Reset use counter
            m_currentSecUseCounterPerMethod.clear();
            m_currentSec = (uint64_t) _useTimeSec;
        }
        
        if (minute > m_currentMin) {
            // next minute arrived. Reset use counters
            m_currentMinUseCounterPerMethod.clear();
            m_currentMin = minute;
        }


        // increment counters

        if (m_currentSecUseCounterPerMethod.count(method) > 0) {
            m_currentSecUseCounterPerMethod[method]++;
        } else {
            m_currentSecUseCounterPerMethod[method] = 1;
        }

        if (m_currentMinUseCounterPerMethod.count(method) > 0) {
            m_currentMinUseCounterPerMethod[method]++;
        } else {
            m_currentMinUseCounterPerMethod[method] = 1;
        }

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

    constexpr uint64_t MAX_UNDDOS_MAP_ENTRIES = 1000000;

    e_high_load_detection_result_t algorithm::register_call_from_origin(
            const char *_origin, const char *_strMethod, time_tick_mark _callTime, duration _durationToPast) {



        if (!m_settings.m_enabled)
            return e_high_load_detection_result_t::ehldr_no_error;
        if (_origin == nullptr || _origin[0] == '\0')
            return e_high_load_detection_result_t::ehldr_bad_origin;

        setCallTimeToNowIfZero(_callTime);


        m_globalOrigin.recordUse(_callTime, _strMethod);

        if (m_globalOrigin.isBanned(_callTime))
            return e_high_load_detection_result_t::ehldr_already_banned;  // still banned

        lock_type lock(x_mtx);

        if (m_trackedOriginsMap.size() > MAX_UNDDOS_MAP_ENTRIES) {
            // the map grows in size, we clear it from time to time
            // so it does not grow indefinitely because of old accesses
            // this will happen very infrequently, you need a million different IPS
            // to fill the map
            m_trackedOriginsMap.clear();
        }


        if (m_trackedOriginsMap.count(_origin) == 0) {
            std::string strOrigin = _origin;
            m_trackedOriginsMap.emplace(strOrigin, tracked_origin(_origin, _callTime));
            return e_high_load_detection_result_t::ehldr_no_error;
        }

        tracked_origin &to = m_trackedOriginsMap.find(_origin)->second;

        to.recordUse(_callTime, _strMethod);

        if (to.isBanned(_callTime))
            return e_high_load_detection_result_t::ehldr_already_banned;  // still banned

        const origin_entry_setting &oe = m_settings.find_origin_entry_setting(_origin);
        auto maxCallsPerMinute = oe.max_calls_per_minute(_strMethod);
        if (maxCallsPerMinute > 0) {
            if (to.m_currentMinUseCounterPerMethod[_strMethod] > maxCallsPerMinute) {
                to.m_banUntilSec = _callTime + oe.m_banPerMinDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_min;  // ban by too high load per min
            }
        }
        auto maxCallsPerSecond = oe.max_calls_per_second(_strMethod);
        if (maxCallsPerSecond > 0) {
            if (to.m_currentSecUseCounterPerMethod[_strMethod] > maxCallsPerSecond) {
                to.m_banUntilSec = _callTime + oe.m_banPerSecDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_sec;
            }
        }


        maxCallsPerMinute = m_settings.m_globalLimitSetting.max_calls_per_minute(_strMethod);
        if (maxCallsPerMinute > 0) {
            if (m_globalOrigin.m_currentMinUseCounterPerMethod[_strMethod] > maxCallsPerMinute) {
                m_globalOrigin.m_banUntilSec = _callTime + m_settings.m_globalLimitSetting.m_banPerMinDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_min;
            }
        }
        maxCallsPerSecond = m_settings.m_globalLimitSetting.max_calls_per_second(_strMethod);
        if (maxCallsPerSecond > 0) {
            if (m_globalOrigin.m_currentSecUseCounterPerMethod[_strMethod] > maxCallsPerSecond) {
                m_globalOrigin.m_banUntilSec = _callTime + m_settings.m_globalLimitSetting.m_banPerSecDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_sec;  // ban by too high load per second
            }
        }

        return e_high_load_detection_result_t::ehldr_no_error;
    }


    e_high_load_detection_result_t algorithm::register_ws_conn_for_origin(const char *origin) {
        if (!m_settings.m_enabled)
            return e_high_load_detection_result_t::ehldr_no_error;
        if (origin == nullptr || origin[0] == '\0')
            return e_high_load_detection_result_t::ehldr_bad_origin;
        lock_type lock(x_mtx);
        ++m_WsConnCountGlobal;
        if (m_WsConnCountGlobal > m_settings.m_globalLimitSetting.m_maxWSConn)
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
