#include <skutils/unddos.h>
#include <shared_mutex>

namespace skutils::unddos {

    origin_dos_limits::origin_dos_limits() {
        clear();
    }

    origin_dos_limits::origin_dos_limits(const origin_dos_limits &other) {
        assign(other);
    }

    origin_dos_limits::origin_dos_limits(origin_dos_limits &&other) {
        assign(other);
        other.clear();
    }

    origin_dos_limits::~origin_dos_limits() {
        clear();
    }

    origin_dos_limits &origin_dos_limits::operator=(const origin_dos_limits &other) {
        assign(other);
        return (*this);
    }

    void origin_dos_limits::load_defaults_for_any_origin() {
        load_friendly_for_any_origin();
        // load_reasonable_for_any_origin();
    }

    void origin_dos_limits::load_friendly_for_any_origin() {
        clear();
        m_originWildcards.push_back("*");
        m_defaultMaxCallsPerSec = 500;
        m_defaultMaxCallsPerMin = 15000;
        m_banPerSecDuration = duration(15);
        m_banPerMinDuration = duration(120);
        m_maxWSConn = 50;
        load_recommended_custom_methods_as_multiplier_of_default();
    }


    void origin_dos_limits::load_unlim_for_any_origin() {
        clear();
        m_originWildcards.push_back("*");
        m_defaultMaxCallsPerSec = std::numeric_limits<size_t>::max();
        m_defaultMaxCallsPerMin = std::numeric_limits<size_t>::max();
        m_banPerSecDuration = duration(0);
        m_banPerMinDuration = duration(0);
        m_maxWSConn = std::numeric_limits<size_t>::max();
        load_recommended_custom_methods_as_multiplier_of_default();
    }

    void origin_dos_limits::load_unlim_for_localhost_only() {
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

    void origin_dos_limits::load_custom_method_as_multiplier_of_default(
            const char *strMethod, double lfMultiplier) {
        if (strMethod == nullptr || strMethod[0] == '\0' || lfMultiplier <= 0.0)
            return;
        custom_method_limits cme;
        cme.m_maxCallsPerSecond = size_t(m_defaultMaxCallsPerSec * lfMultiplier);
        cme.m_maxCallsPerMinute = size_t(m_defaultMaxCallsPerMin * lfMultiplier);
        m_mapCustomMethodLimits[strMethod] = cme;
    }

    void origin_dos_limits::load_recommended_custom_methods_as_multiplier_of_default(
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


    bool origin_dos_limits::empty() const {
        if (!m_originWildcards.empty())
            return false;
        return true;
    }

    void origin_dos_limits::clear() {
        m_originWildcards.clear();
        m_defaultMaxCallsPerSec = 0;
        m_defaultMaxCallsPerMin = 0;
        m_banPerSecDuration = duration(0);
        m_banPerMinDuration = duration(0);
        m_maxWSConn = 0;
        m_mapCustomMethodLimits.clear();
    }

    origin_dos_limits &origin_dos_limits::assign(const origin_dos_limits &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        clear();
        m_originWildcards = other.m_originWildcards;
        m_defaultMaxCallsPerSec = other.m_defaultMaxCallsPerSec;
        m_defaultMaxCallsPerMin = other.m_defaultMaxCallsPerMin;
        m_banPerSecDuration = other.m_banPerSecDuration;
        m_banPerMinDuration = other.m_banPerMinDuration;
        m_maxWSConn = other.m_maxWSConn;
        m_mapCustomMethodLimits = other.m_mapCustomMethodLimits;
        return (*this);
    }

    origin_dos_limits &origin_dos_limits::merge(const origin_dos_limits &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        if (m_originWildcards != other.m_originWildcards)
            return (*this);
        m_defaultMaxCallsPerSec = std::min(m_defaultMaxCallsPerSec, other.m_defaultMaxCallsPerSec);
        m_defaultMaxCallsPerMin = std::min(m_defaultMaxCallsPerMin, other.m_defaultMaxCallsPerMin);
        m_banPerSecDuration = std::max(m_banPerSecDuration, other.m_banPerSecDuration);
        m_banPerMinDuration = std::max(m_banPerMinDuration, other.m_banPerMinDuration);
        m_maxWSConn = std::min(m_maxWSConn, other.m_maxWSConn);
        if (!other.m_mapCustomMethodLimits.empty()) {
            nlohmann::json joCMS = nlohmann::json::object();
            map_custom_method_limits_t::const_iterator itWalk =
                    other.m_mapCustomMethodLimits.cbegin(),
                    itEnd =
                    other.m_mapCustomMethodLimits.cend();
            for (; itWalk != itEnd; ++itWalk) {
                const custom_method_limits &cme = itWalk->second;
                map_custom_method_limits_t::iterator itFind =
                        m_mapCustomMethodLimits.find(itWalk->first);
                if (itFind != m_mapCustomMethodLimits.end()) {
                    itFind->second.merge(cme);  // merge with existing
                    continue;
                }
                m_mapCustomMethodLimits[itWalk->first] = cme;  // add mew
            }
        }
        return (*this);
    }

    void origin_dos_limits::fromJSON(const nlohmann::json &jo) {
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
                custom_method_limits cme;
                if (joMethod.find("max_calls_per_second") != jo.end())
                    cme.m_maxCallsPerSecond = joMethod["max_calls_per_second"].get<size_t>();
                if (joMethod.find("max_calls_per_minute") != jo.end())
                    cme.m_maxCallsPerMinute = joMethod["max_calls_per_minute"].get<size_t>();
                m_mapCustomMethodLimits[it.key()] = cme;
            }
        }
    }

    void origin_dos_limits::toJSON(nlohmann::json &jo) const {
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
        if (!m_mapCustomMethodLimits.empty()) {
            nlohmann::json joCMS = nlohmann::json::object();
            map_custom_method_limits_t::const_iterator itWalk = m_mapCustomMethodLimits.cbegin(),
                    itEnd = m_mapCustomMethodLimits.cend();
            for (; itWalk != itEnd; ++itWalk) {
                const custom_method_limits &cme = itWalk->second;
                nlohmann::json joMethod = nlohmann::json::object();
                joMethod["max_calls_per_second"] = cme.m_maxCallsPerSecond;
                joMethod["max_calls_per_minute"] = cme.m_maxCallsPerMinute;
                joCMS[itWalk->first] = joMethod;
            }
            jo["custom_method_settings"] = joCMS;
        }
    }

    bool origin_dos_limits::match_origin(const char *origin) const {
        if (origin == nullptr || (*origin) == '\0')
            return false;
        for (const std::string &wildcard: m_originWildcards) {
            if (skutils::tools::wildcmp(wildcard.c_str(), origin))
                return true;
        }
        return false;
    }

    size_t origin_dos_limits::max_calls_per_second(const char *strMethod) const {
        if (strMethod == nullptr || strMethod[0] == '\0')
            return m_defaultMaxCallsPerSec;
        map_custom_method_limits_t::const_iterator itFind =
                m_mapCustomMethodLimits.find(strMethod),
                itEnd = m_mapCustomMethodLimits.cend();
        if (itFind == itEnd)
            return m_defaultMaxCallsPerSec;
        const custom_method_limits &cme = itFind->second;
        const size_t cnt = std::max(m_defaultMaxCallsPerSec, cme.m_maxCallsPerSecond);
        return cnt;
    }

    size_t origin_dos_limits::max_calls_per_minute(const char *strMethod) const {
        if (strMethod == nullptr || strMethod[0] == '\0')
            return m_defaultMaxCallsPerMin;
        map_custom_method_limits_t::const_iterator itFind =
                m_mapCustomMethodLimits.find(strMethod),
                itEnd = m_mapCustomMethodLimits.cend();
        if (itFind == itEnd)
            return m_defaultMaxCallsPerMin;
        const custom_method_limits &cme = itFind->second;
        const size_t cnt = std::max(m_defaultMaxCallsPerMin, cme.m_maxCallsPerMinute);
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
        if (!m_originDosLimits.empty())
            return false;
        if (!m_globalLimitSetting.empty())
            return false;
        return true;
    }

    void settings::clear() {
        m_enabled = true;
        m_originDosLimits.clear();
        m_globalLimitSetting.clear();
    }

    settings &settings::assign(const settings &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        clear();
        m_enabled = other.m_enabled;
        m_originDosLimits = other.m_originDosLimits;
        m_globalLimitSetting = other.m_globalLimitSetting;
        return (*this);
    }

    settings &settings::merge(const settings &other) {
        if (((void *) (this)) == ((void *) (&other)))
            return (*this);
        for (const origin_dos_limits &oe: other.m_originDosLimits)
            merge(oe);
        m_globalLimitSetting.merge(other.m_globalLimitSetting);
        return (*this);
    }

    settings &settings::merge(const origin_dos_limits &oe) {
        size_t i = indexOfOrigin(oe);
        if (i == std::string::npos)
            m_originDosLimits.push_back(oe);
        else
            m_originDosLimits[i].merge(oe);
        return (*this);
    }

    size_t settings::indexOfOrigin(const origin_dos_limits &oe, size_t idxStart) {
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
        size_t cnt = m_originDosLimits.size();
        size_t i = (idxStart == std::string::npos) ? 0 : (idxStart + 1);
        for (; i < cnt; ++i) {
            const origin_dos_limits &oe = m_originDosLimits[i];
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
                    origin_dos_limits oe;
                    oe.fromJSON(joOrigin);
                    m_originDosLimits.push_back(oe);
                }
            }
        }
        if (jo.find("global") != jo.end()) {
            const nlohmann::json &joGlobalLimit = jo["global"];
            origin_dos_limits oe;
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
        for (const origin_dos_limits &oe: m_originDosLimits) {
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

    size_t settings::findOriginLimitsMatch(const char *origin, size_t idxStart) const {
        if (origin == nullptr || (*origin) == '\0')
            return std::string::npos;
        size_t cnt = m_originDosLimits.size();
        size_t i = (idxStart == std::string::npos) ? 0 : (idxStart + 1);
        for (; i < cnt; ++i) {
            const origin_dos_limits &oe = m_originDosLimits[i];
            if (oe.match_origin(origin))
                return i;
        }
        return std::string::npos;
    }

    origin_dos_limits &settings::findOriginDosLimits(const char *_origin) {
        size_t i = findOriginLimitsMatch(_origin);
        if (i != std::string::npos)
            return m_originDosLimits[i];
        return auto_append_any_origin_rule();
    }

    origin_dos_limits &settings::auto_append_any_origin_rule() {
        if (!m_originDosLimits.empty()) {
            size_t i = findOriginLimitsMatch("*");
            if (i != std::string::npos)
                return m_originDosLimits[i];
        }
        origin_dos_limits oe;
        oe.load_defaults_for_any_origin();
        m_originDosLimits.push_back(oe);
        return m_originDosLimits[m_originDosLimits.size() - 1];
    }


    tracked_origin::tracked_origin(const char *origin)
            : m_origin((origin != nullptr && origin[0] != '\0') ? origin : "") {
    }


    void tracked_origin::setDosLimits(const origin_dos_limits &_dosLimits) {
        m_dosLimits = _dosLimits;
    }

    e_high_load_detection_result_t
    tracked_origin::recordMethodUseAndDetectBan(uint64_t _callTimeSec, const char *_strMethod) {
        recordUse(_callTimeSec, _strMethod);

        if (isBanned(_callTimeSec)) {
            return e_high_load_detection_result_t::ehldr_already_banned;  // still banned
        }

        return detectBan(_callTimeSec, _strMethod);

    }

    e_high_load_detection_result_t tracked_origin::detectBan(uint64_t _callTimeSec, const char *_strMethod) {

        auto maxCallsPerMinute = m_dosLimits.max_calls_per_minute(_strMethod);

        std::string method = (_strMethod? _strMethod : "");

        if (maxCallsPerMinute > 0) {
            if (m_currentMinUseCounterPerMethod[method] > maxCallsPerMinute) {
                m_banUntilSec = _callTimeSec + m_dosLimits.m_banPerMinDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_min;  // ban by too high load per min
            }
        }

        auto maxCallsPerSecond = m_dosLimits.max_calls_per_second(method.c_str());
        if (maxCallsPerSecond > 0) {
            if (m_currentSecUseCounterPerMethod[method] > maxCallsPerSecond) {
                m_banUntilSec = _callTimeSec + m_dosLimits.m_banPerSecDuration;
                return e_high_load_detection_result_t::ehldr_detected_ban_per_sec;
            }
        }

        return e_high_load_detection_result_t::ehldr_no_error;
    }


    tracked_origin::~tracked_origin() {
    }


    bool tracked_origin::isBanned(uint64_t _timeSec) {
        return (_timeSec <= m_banUntilSec);
    }

    void tracked_origin::recordUse(uint64_t _useTimeSec, const char *_strMethod) {

        std::string method = (_strMethod ? _strMethod : "");


        static constexpr uint64_t SECONDS_IN_MINUTE = 60;
        auto minute = _useTimeSec / SECONDS_IN_MINUTE;

        std::lock_guard lock(x_mutex);

        if ((uint64_t) _useTimeSec > m_currentSec) {
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

    algorithm::algorithm() : m_globalOrigin(nullptr) {}

    algorithm::algorithm(const settings &st) : m_globalOrigin(nullptr) {
        m_settings = st;
        m_globalOrigin.setDosLimits(m_settings.m_globalLimitSetting);
    }

    algorithm::~algorithm() {}

    algorithm &algorithm::operator=(const settings &st) {
        m_settings = st;
        return (*this);
    }

    constexpr uint64_t MAX_UNDDOS_MAP_ENTRIES = 256 * 1024;

    e_high_load_detection_result_t algorithm::register_call_from_origin(
            const char *_origin, const char *_strMethod, time_tick_mark _callTime, duration) {


        if (!m_settings.m_enabled) {
            // DOS protection disabled
            return e_high_load_detection_result_t::ehldr_no_error;
        }

        if (_origin == nullptr || _origin[0] == '\0')
            return e_high_load_detection_result_t::ehldr_bad_origin;

        // set the call time to current time if it was not provided
        setCallTimeToNowIfZero(_callTime);

        // first check for global ban since it does not need to acces the map

        auto result = m_globalOrigin.recordMethodUseAndDetectBan(_callTime, _strMethod);

        if (result != e_high_load_detection_result_t::ehldr_no_error)
            return result;

        // now we checked for global ban, check for a ban based on origin
        // we need to read lock to do it
        std::shared_ptr<tracked_origin> trackedOrigin = nullptr;
        {
            std::shared_lock<std::shared_mutex> lock(x_mtx);
            auto iterator = m_trackedOriginsMap.find(_origin);
            if (iterator != m_trackedOriginsMap.end()) {
                trackedOrigin = iterator->second;
            }
        }

        // if we did not find the tracked origin, it is not in the map yet. We need to init it under write lock

        if (!trackedOrigin) {
            addNewOriginToMap(_origin);
            return e_high_load_detection_result_t::ehldr_no_error;
        } else {
            // since we now have trackedOrigin the rest can be done without holding any lock on the map
            return trackedOrigin->recordMethodUseAndDetectBan(_callTime, _strMethod);
        }

    }

    void algorithm::addNewOriginToMap(const char *_origin) {
        const origin_dos_limits &oe = m_settings.findOriginDosLimits(_origin);
        {
            std::unique_lock<std::shared_mutex> writeLock(x_mtx);
            if (m_trackedOriginsMap.size() > MAX_UNDDOS_MAP_ENTRIES) {
                // the map grows in size, we clear it from time to time
                // so that it does not grow indefinitely because of old accesses
                // that will happen very infrequently
                // to fill the map
                m_trackedOriginsMap.clear();
            }
            if (m_trackedOriginsMap.count(_origin) == 0) {
                m_trackedOriginsMap.emplace(_origin, std::make_shared<tracked_origin>(_origin));
                m_trackedOriginsMap.at(_origin)->setDosLimits(oe);

            }
        }
    }


    e_high_load_detection_result_t algorithm::register_ws_conn_for_origin(const char *origin) {
        if (!m_settings.m_enabled)
            return e_high_load_detection_result_t::ehldr_no_error;
        if (origin == nullptr || origin[0] == '\0')
            return e_high_load_detection_result_t::ehldr_bad_origin;
        std::unique_lock<std::shared_mutex> lock(x_mtx);
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
        const origin_dos_limits &oe = m_settings.findOriginDosLimits(origin);
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
        std::unique_lock<std::shared_mutex> lock(x_mtx);
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
        std::unique_lock<std::shared_mutex> lock(x_mtx);
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
        std::shared_lock<std::shared_mutex> lock(x_mtx);
        m_settings.auto_append_any_origin_rule();
        settings copied = m_settings;
        return copied;
    }

    void algorithm::set_settings(const settings &new_settings) const {
        std::unique_lock<std::shared_mutex> lock(x_mtx);
        m_settings = new_settings;
        m_settings.auto_append_any_origin_rule();
    }

    nlohmann::json algorithm::get_settings_json() const {
        std::shared_lock<std::shared_mutex> lock(x_mtx);
        m_settings.auto_append_any_origin_rule();
        nlohmann::json joUnDdosSettings = nlohmann::json::object();
        m_settings.toJSON(joUnDdosSettings);
        return joUnDdosSettings;
    }

};  // namespace skutils::unddos
