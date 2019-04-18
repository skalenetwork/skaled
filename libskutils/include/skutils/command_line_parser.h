#if ( !defined __SKUTILS_COMMAND_LINE_PARSER_H )
#define __SKUTILS_COMMAND_LINE_PARSER_H 1

#include <functional>
#include <list>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace skutils {
namespace command_line {

class parser {
    std::string app_name_, app_version_;

public:
    typedef std::vector< std::string > vec_args_t;
    typedef vec_args_t::const_iterator const_iterator;
    typedef std::function< void() > flag_argument_handler_t;
    typedef std::function< void( const std::string& strValue ) > string_argument_value_handler_t;
    // typedef std::set < std::string > set_names_t;
private:
    typedef std::map< std::string, flag_argument_handler_t > map_flag_handlers_t;
    typedef std::map< std::string, string_argument_value_handler_t > map_value_handlers_t;
    map_flag_handlers_t flag_handlers_;
    map_value_handlers_t value_handlers_;
    typedef std::list< std::string > name_sequence_list_t;
    name_sequence_list_t list_sequence_handlers_;
    name_sequence_list_t list_sequence_categories_;
    typedef std::map< std::string, std::string > map_name2name_t;
    map_name2name_t map_opt2cat_;
    typedef std::map< std::string, name_sequence_list_t > map_category_opts_t;
    map_category_opts_t map_category_opts_;
    string_argument_value_handler_t default_handler_;
    string_argument_value_handler_t unknown_handler_;
    string_argument_value_handler_t arg_without_value_handler_;
    vec_args_t args_;
    size_t errorArgIndex_;
    typedef std::map< std::string, std::string > map_descriptions_t;
    map_descriptions_t map_descriptions_;

public:
    std::string strDefaultCategoryName_;  // default is "Other", should not be empty
    char chrCateoryAndNameDelimiter_;     // default is '\n'
    parser( const char* strAppName = nullptr, const char* strAppVersion = nullptr,
        const char* strDefaultCategoryName = nullptr,  // default is "Other", should not be empty
        char chrCateoryAndNameDelimiter = '\n'         // default is '\n'
    );
    virtual ~parser();
    parser( const parser& ) = delete;
    parser( parser&& ) = delete;
    parser& operator=( const parser& ) = delete;
    bool empty() const;
    void clear();
    void clear_model();
    std::string& app_name() { return app_name_; }
    const std::string& app_name() const { return app_name_; }
    std::string& app_version() { return app_version_; }
    const std::string& app_version() const { return app_version_; }
    size_t errorArgIndex() const { return errorArgIndex_; }
    size_t size() const { return args_.size(); }
    bool haveParseError() const {
        return ( errorArgIndex() != size_t( -1 ) || empty() ) ? true : false;
    }
    std::string& get( size_t i ) { return args_[i]; }
    const std::string& get( size_t i ) const { return args_[i]; }
    std::string& operator[]( size_t i ) { return get( i ); }
    const std::string& operator[]( size_t i ) const { return get( i ); }
    std::string& front() { return get( 0 ); }              // executable path
    const std::string& front() const { return get( 0 ); }  // executable path
    virtual bool parse( int argc, char** argv );
    const_iterator cbegin() const { return args_.cbegin(); }
    const_iterator cend() const { return args_.cend(); }

private:
    static bool stat_detect_arg_name(
        std::string& str, std::string& strExractedValueAfterEquality );

public:
    bool describe( const char* name, const char* desc );  // describes only existing
    bool describe( const std::string& name, const std::string& desc ) {
        return describe( name.c_str(), desc.c_str() );
    }  // describes only existing
    std::string description( const char* name ) const;
    std::string description( const std::string& name ) const { return description( name.c_str() ); }
    //
    flag_argument_handler_t flag_handler( const char* name ) { return flag_handlers_[name]; }
    string_argument_value_handler_t value_handler( const char* name ) {
        return value_handlers_[name];
    }
    flag_argument_handler_t flag_handler( const std::string& name ) {
        return flag_handler( name.c_str() );
    }
    string_argument_value_handler_t value_handler( const std::string& name ) {
        return value_handler( name.c_str() );
    }
    //
    bool flag_handler( const char* name, const char* desc, flag_argument_handler_t fn );
    bool value_handler( const char* name, const char* desc, string_argument_value_handler_t fn );
    bool remove_entry( const char* name );
    bool remove_entry( const std::string& name ) { return remove_entry( name.c_str() ); }
    //
    void flag_handler( const char* name, flag_argument_handler_t fn ) {
        flag_handler( name, nullptr, fn );
    }
    void value_handler( const char* name, string_argument_value_handler_t fn ) {
        value_handler( name, nullptr, fn );
    }
    void flag_handler(
        const std::string& name, const std::string& desc, flag_argument_handler_t fn ) {
        flag_handler( name.c_str(), desc.c_str(), fn );
    }
    void value_handler(
        const std::string& name, const std::string& desc, string_argument_value_handler_t fn ) {
        value_handler( name.c_str(), desc.c_str(), fn );
    }
    void flag_handler( const std::string& name, flag_argument_handler_t fn ) {
        flag_handler( name.c_str(), fn );
    }
    void value_handler( const std::string& name, string_argument_value_handler_t fn ) {
        value_handler( name.c_str(), fn );
    }
    bool have_flag_handler( const char* name ) const;
    bool have_value_handler( const char* name ) const;
    bool have_any_handler( const char* name ) const {
        return ( have_flag_handler( name ) || have_value_handler( name ) ) ? true : false;
    }
    bool have_flag_handler( const std::string& name ) const {
        return have_flag_handler( name.c_str() );
    }
    bool have_value_handler( const std::string& name ) const {
        return have_value_handler( name.c_str() );
    }
    bool have_any_handler( const std::string& name ) const {
        return have_any_handler( name.c_str() );
    }
    string_argument_value_handler_t default_handler() { return default_handler_; }
    void default_handler( string_argument_value_handler_t fn ) { default_handler_ = fn; }
    string_argument_value_handler_t unknown_handler() { return unknown_handler_; }
    void unknown_handler( string_argument_value_handler_t fn ) { unknown_handler_ = fn; }
    string_argument_value_handler_t arg_without_value_handler() {
        return arg_without_value_handler_;
    }
    void arg_without_value_handler( string_argument_value_handler_t fn ) {
        arg_without_value_handler_ = fn;
    }
    virtual void on_unknown_arg();
    virtual void on_arg_without_value();
    virtual void on_arg_value( const std::string& s, const std::string& v );
    virtual void on_arg_flag( const std::string& s );
    virtual bool on_default_value( const std::string& v );
    parser& on( const char* name, flag_argument_handler_t fn ) {
        flag_handler( name, fn );
        return ( *this );
    }
    parser& on( const char* name, string_argument_value_handler_t fn ) {
        value_handler( name, fn );
        return ( *this );
    }
    parser& on( const char* name, const char* desc, flag_argument_handler_t fn ) {
        flag_handler( name, desc, fn );
        return ( *this );
    }
    parser& on( const char* name, const char* desc, string_argument_value_handler_t fn ) {
        value_handler( name, desc, fn );
        return ( *this );
    }
    parser& on( const std::string& name, flag_argument_handler_t fn ) {
        flag_handler( name, fn );
        return ( *this );
    }
    parser& on( const std::string& name, string_argument_value_handler_t fn ) {
        value_handler( name, fn );
        return ( *this );
    }
    parser& on( const std::string& name, const std::string& desc, flag_argument_handler_t fn ) {
        flag_handler( name, desc, fn );
        return ( *this );
    }
    parser& on(
        const std::string& name, const std::string& desc, string_argument_value_handler_t fn ) {
        value_handler( name, desc, fn );
        return ( *this );
    }
    parser& on_default( string_argument_value_handler_t fn ) {
        default_handler( fn );
        return ( *this );
    }
    parser& on( string_argument_value_handler_t fn ) { return on_default( fn ); }
    parser& on_unknown( string_argument_value_handler_t fn ) {
        unknown_handler( fn );
        return ( *this );
    }
    parser& on_arg_without_value( string_argument_value_handler_t fn ) {
        arg_without_value_handler( fn );
        return ( *this );
    }
    template < typename N, typename F >
    parser& operator()( N name, F fn ) {
        return on( name, fn );
    }
    template < typename N, typename F >
    parser& operator()( N name, N desc, F fn ) {
        return on( name, desc, fn );
    }
    parser& operator()( string_argument_value_handler_t fn ) { return on_default( fn ); }
    enum class e_options_publishing_type_t {
        eoptt_auto,
        eoptt_linear,
        eoptt_categorized
    };  /// enum class e_options_publishing_type_t
    virtual std::string banner_text() const;
    virtual std::string options_text(
        e_options_publishing_type_t eoptt = e_options_publishing_type_t::eoptt_auto ) const;
    virtual std::string help_text(
        e_options_publishing_type_t eoptt = e_options_publishing_type_t::eoptt_auto ) const;
    // templated value_handler() versions
    template < typename T >
    void value_handler(
        const char* name, const char* desc, std::function< void( const T& aValue ) > fn ) {
        value_handler( name, desc, [&]( const std::string& strValue ) {
            std::istringstream ss;
            T t;
            ss >> t;
            fn( t );
        } );
    }
    template < typename T >
    void value_handler(
        const std::string& name, std::string& desc, std::function< void( const T& aValue ) > fn ) {
        value_handler< T >( name.c_str(), desc.c_str(), fn );
    }
    template < typename T >
    void value_handler( const char* name, std::function< void( const T& aValue ) > fn ) {
        value_handler< T >( name, nullptr, fn );
    }
    template < typename T >
    void value_handler( const std::string& name, std::function< void( const T& aValue ) > fn ) {
        value_handler< T >( name.c_str(), nullptr, fn );
    }
    // templated on() versions
    template < typename T >
    parser& on( const char* name, const char* desc, std::function< void( const T& aValue ) > fn ) {
        value_handler< T >( name, desc, fn );
        return ( *this );
    }
    template < typename T >
    parser& on(
        const std::string& name, std::string& desc, std::function< void( const T& aValue ) > fn ) {
        value_handler< T >( name.c_str(), desc.c_str(), fn );
        return ( *this );
    }
    template < typename T >
    parser& on( const char* name, std::function< void( const T& aValue ) > fn ) {
        value_handler< T >( name, nullptr, fn );
        return ( *this );
    }
    template < typename T >
    parser& on( const std::string& name, std::function< void( const T& aValue ) > fn ) {
        value_handler< T >( name.c_str(), nullptr, fn );
        return ( *this );
    }

    static void stat_network_interfaces_info( std::ostream& os );
};  /// class parser

};  // namespace command_line
};  // namespace skutils

#endif  /// (!defined __SKUTILS_COMMAND_LINE_PARSER_H)
