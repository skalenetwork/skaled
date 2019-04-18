#include <skutils/command_line_parser.h>
#include <skutils/console_colors.h>
#include <skutils/utils.h>
//#include <skutils/ws.h>
#include <skutils/network.h>
#include <algorithm>

namespace skutils {
namespace command_line {

parser::parser( const char* strAppName,  // = nullptr
    const char* strAppVersion,           // = nullptr
    const char* strDefaultCategoryName,  // = nullptr // default is "Other", should not be empty
    char chrCateoryAndNameDelimiter      // = '\n' // default is '\n'
    )
    : app_name_( ( strAppName != nullptr && strAppName[0] != '\0' ) ? strAppName : "" ),
      app_version_( ( strAppVersion != nullptr && strAppVersion[0] != '\0' ) ? strAppVersion : "" ),
      strDefaultCategoryName_(
          ( strDefaultCategoryName != nullptr && strDefaultCategoryName[0] != '\0' ) ?
              strDefaultCategoryName :
              "Other" )  // default is "Other", should not be empty
      ,
      chrCateoryAndNameDelimiter_( chrCateoryAndNameDelimiter )  // default is '\n'
{
    clear();
}
parser::~parser() {
    clear();
    clear_model();
}

bool parser::empty() const {
    return ( args_.empty() ||
               ( flag_handlers_.empty() && value_handlers_.empty() && ( !default_handler_ ) ) ) ?
               true :
               false;
}

void parser::clear() {
    errorArgIndex_ = size_t( -1 );
    args_.clear();
    // clear_model();
}

void parser::clear_model() {
    flag_handlers_.clear();
    value_handlers_.clear();
    default_handler_ = string_argument_value_handler_t( nullptr );
    unknown_handler_ = string_argument_value_handler_t( nullptr );
    arg_without_value_handler_ = string_argument_value_handler_t( nullptr );
    map_descriptions_.clear();
    list_sequence_handlers_.clear();
    list_sequence_categories_.clear();
    map_opt2cat_.clear();
    map_category_opts_.clear();
}

bool parser::parse( int argc, char** argv ) {
    clear();
    if ( argc < 1 )
        return false;
    for ( int argi = 0; argi < argc; ++argi )
        args_.push_back( argv[argi] );
    const_iterator itWalk = cbegin(), itEnd = cend();
    for ( ++itWalk; itWalk != itEnd; ++itWalk ) {
        std::string str = ( *itWalk ), strExractedValueAfterEquality;
        if ( !stat_detect_arg_name( str, strExractedValueAfterEquality ) ) {
            if ( on_default_value( str ) ) {
                continue;
            }
            errorArgIndex_ = size_t( std::distance( cbegin(), itWalk ) );
            on_unknown_arg();
            return false;
        }
        bool bHaveValueHandler = have_value_handler( str );
        if ( !strExractedValueAfterEquality.empty() ) {
            // try only value handlers
            if ( !bHaveValueHandler ) {
                errorArgIndex_ = size_t( std::distance( cbegin(), itWalk ) );
                on_unknown_arg();
                return false;
            }
            on_arg_value( str, strExractedValueAfterEquality );
            continue;
        }  // if( ! strExractedValueAfterEquality.empty() )
        // argument name is detected
        if ( bHaveValueHandler ) {
            ++itWalk;
            if ( itWalk == itEnd ) {
                --itWalk;
                errorArgIndex_ = size_t( std::distance( cbegin(), itWalk ) );
                on_arg_without_value();
                return false;
            }
            std::string v = ( *itWalk );
            on_arg_value( str, v );
            continue;
        }  // if( if( bHaveValueHandler ) )
        bool bHaveFlagHandler = have_flag_handler( str );
        if ( bHaveFlagHandler ) {
            on_arg_flag( str );
            continue;
        }  // if( bHaveFlagHandler )
        errorArgIndex_ = size_t( std::distance( cbegin(), itWalk ) );
        on_unknown_arg();
        return false;
    }  // for( ++itWalk; itWalk != itEnd; ++itWalk )
    return true;
}

bool parser::stat_detect_arg_name( std::string& str, std::string& strExractedValueAfterEquality ) {
    strExractedValueAfterEquality.clear();
    std::string out = str;
    if ( out.empty() )
        return false;
    if ( out[0] != '-' )
        return false;
    // remove first '-'
    out.erase( out.begin() );
    if ( out.empty() )
        return false;
    if ( out[0] != '-' ) {
        str = out;
        return true;
    }
    // remove second '-'
    out.erase( out.begin() );
    if ( out.empty() )
        return false;
    // find '='
    size_t nPos = out.find( '=' );
    if ( nPos != std::string::npos && nPos > 0 ) {
        size_t cnt = out.length();
        strExractedValueAfterEquality = out.substr( nPos + 1, cnt - nPos - 1 );
        out = out.substr( 0, nPos );
    }
    str = out;
    return true;
}

bool parser::describe( const char* name, const char* desc ) {  // describes only existing
    if ( name == nullptr || name[0] == '\0' || desc == nullptr || desc[0] == '\0' )
        return false;
    skutils::string_list_t listParts = skutils::tools::split( name, chrCateoryAndNameDelimiter_ );
    size_t cntParts = listParts.size();
    if ( !( 1 <= cntParts && cntParts <= 2 ) )
        return false;
    std::string /*strCategory( ( cntParts == 2 ) ? listParts.front() : strDefaultCategoryName_ ),*/
        strName( listParts.back() );
    if ( !have_any_handler( strName ) )
        return false;  // describes only existing
    map_descriptions_[strName] = desc;
    return true;
}

std::string parser::description( const char* name ) const {
    std::string desc;
    if ( name == nullptr || name[0] == '\0' )
        return desc;
    skutils::string_list_t listParts = skutils::tools::split( name, chrCateoryAndNameDelimiter_ );
    size_t cntParts = listParts.size();
    if ( !( 1 <= cntParts && cntParts <= 2 ) )
        return desc;
    std::string /*strCategory( ( cntParts == 2 ) ? listParts.front() : strDefaultCategoryName_ ),*/
        strName( listParts.back() );
    map_descriptions_t::const_iterator itFind = map_descriptions_.find( strName ),
                                       itEnd = map_descriptions_.cend();
    if ( itFind == itEnd )
        return desc;
    desc = itFind->second;
    return desc;
}

bool parser::have_flag_handler( const char* name ) const {
    map_flag_handlers_t::const_iterator itFind = flag_handlers_.find( name ),
                                        itEnd = flag_handlers_.cend();
    if ( itFind == itEnd )
        return false;
    flag_argument_handler_t fn = itFind->second;
    if ( !fn )
        return false;
    return true;
}
bool parser::have_value_handler( const char* name ) const {
    map_value_handlers_t::const_iterator itFind = value_handlers_.find( name ),
                                         itEnd = value_handlers_.cend();
    if ( itFind == itEnd )
        return false;
    string_argument_value_handler_t fn = itFind->second;
    if ( !fn )
        return false;
    return true;
}

void parser::on_unknown_arg() {
    string_argument_value_handler_t fn = unknown_handler();
    if ( fn )
        fn( get( errorArgIndex() ) );
}
void parser::on_arg_without_value() {
    string_argument_value_handler_t fn = arg_without_value_handler();
    if ( fn )
        fn( get( errorArgIndex() ) );
}
void parser::on_arg_value( const std::string& s, const std::string& v ) {
    string_argument_value_handler_t fn = value_handler( s );
    fn( v );
}
void parser::on_arg_flag( const std::string& s ) {
    flag_argument_handler_t fn = flag_handler( s );
    fn();
}
bool parser::on_default_value( const std::string& v ) {
    string_argument_value_handler_t fn = default_handler();
    if ( !fn )
        return false;
    fn( v );
    return true;
}

std::string parser::banner_text() const {
    const std::string &a = app_name(), &v = app_version();
    if ( v.empty() )
        return cc::note( a ) + "\n";
    return cc::note( a ) + cc::debug( " version " ) + cc::note( v ) + cc::debug( "." ) + "\n";
}
std::string parser::options_text(
    parser::e_options_publishing_type_t eoptt  // = parser::e_options_publishing_type_t::eoptt_auto
    ) const {
    if ( list_sequence_handlers_.empty() )
        return "";
    parser::e_options_publishing_type_t eoptt_effective = eoptt;
    if ( eoptt_effective == e_options_publishing_type_t::eoptt_auto ) {
        size_t cntCategories = map_category_opts_.size();
        if ( cntCategories == 0 )
            eoptt_effective = e_options_publishing_type_t::eoptt_linear;
        else if ( cntCategories == 1 &&
                  map_category_opts_.find( strDefaultCategoryName_ ) != map_category_opts_.end() )
            eoptt_effective = e_options_publishing_type_t::eoptt_linear;
        else
            eoptt_effective = e_options_publishing_type_t::eoptt_categorized;
    }
    static const char option_space_prefix[] = "  ", option_space_suffix[] = "...",
                      value_suffix1[] = "=", value_suffix2[] = "<value>";
    size_t name_len_max = 0;
    std::for_each( list_sequence_handlers_.cbegin(), list_sequence_handlers_.cend(),
        [&]( name_sequence_list_t::const_reference name ) {
            std::string nameXnb = "--" + name;
            if ( const_cast< parser* >( this )->value_handler( name ) ) {
                nameXnb += value_suffix1;
                nameXnb += value_suffix2;
            }
            size_t name_len = nameXnb.length();
            if ( name_len_max < name_len )
                name_len_max = name_len;
        } );
    std::string s;
    if ( eoptt_effective == e_options_publishing_type_t::eoptt_linear ) {
        // linear options publishing
        s += cc::note( "Options:" ) + "\n";
        std::for_each( list_sequence_handlers_.cbegin(), list_sequence_handlers_.cend(),
            [&]( name_sequence_list_t::const_reference name ) {
                std::string nameXnb = "--" + name;
                std::string nameX = cc::debug( "--" ) + cc::note( name );
                if ( const_cast< parser* >( this )->value_handler( name ) ) {
                    nameXnb += value_suffix1;
                    nameXnb += value_suffix2;
                    nameX += cc::info( value_suffix1 );
                    nameX += cc::warn( value_suffix2 );
                }
                s += cc::debug( option_space_prefix );
                s += nameX;
                std::string strSpace;
                for ( size_t name_len = nameXnb.length(); name_len < name_len_max; ++name_len )
                    strSpace += ".";
                s += cc::debug( strSpace );
                s += cc::debug( option_space_suffix );
                s += cc::info( description( name ) ) + "\n";
            } );
    } else {
        // categorized options publishing
        std::for_each( list_sequence_categories_.cbegin(), list_sequence_categories_.cend(),
            [&]( name_sequence_list_t::const_reference strCategory ) {
                s += cc::sunny( strCategory ) + cc::note( " options:" ) + "\n";
                const name_sequence_list_t& cat_opts =
                    ( const_cast< parser* >( this ) )->map_category_opts_[strCategory];
                std::for_each( cat_opts.cbegin(), cat_opts.cend(),
                    [&]( name_sequence_list_t::const_reference name ) {
                        std::string nameXnb = "--" + name;
                        std::string nameX = cc::debug( "--" ) + cc::note( name );
                        if ( const_cast< parser* >( this )->value_handler( name ) ) {
                            nameXnb += value_suffix1;
                            nameXnb += value_suffix2;
                            nameX += cc::info( value_suffix1 );
                            nameX += cc::warn( value_suffix2 );
                        }
                        s += cc::debug( option_space_prefix );
                        s += nameX;
                        std::string strSpace;
                        for ( size_t name_len = nameXnb.length(); name_len < name_len_max;
                              ++name_len )
                            strSpace += ".";
                        s += cc::debug( strSpace );
                        s += cc::debug( option_space_suffix );
                        s += cc::info( description( name ) ) + "\n";
                    } );
            } );
    }
    return s;
}
std::string parser::help_text(
    parser::e_options_publishing_type_t eoptt  // = parser::e_options_publishing_type_t::eoptt_auto
    ) const {
    std::string b = banner_text(), o = options_text( eoptt );
    if ( b.empty() )
        return o;
    if ( o.empty() )
        return "";
    return b + o;
}

bool parser::flag_handler(
    const char* name, const char* desc, parser::flag_argument_handler_t fn ) {
    if ( name == nullptr || name[0] == '\0' )
        return false;
    if ( !fn )
        return false;
    skutils::string_list_t listParts = skutils::tools::split( name, chrCateoryAndNameDelimiter_ );
    size_t cntParts = listParts.size();
    if ( !( 1 <= cntParts && cntParts <= 2 ) )
        return false;
    std::string strCategory( ( cntParts == 2 ) ? listParts.front() : strDefaultCategoryName_ ),
        strName( listParts.back() );
    remove_entry( strName );
    map_opt2cat_[strName] = strCategory;
    name_sequence_list_t& cat_opts = map_category_opts_[strCategory];
    cat_opts.push_back( strName );
    flag_handlers_[strName] = fn;
    list_sequence_handlers_.push_back( strName );
    name_sequence_list_t::const_iterator itCatFind = std::find( list_sequence_categories_.cbegin(),
                                             list_sequence_categories_.cend(), strCategory ),
                                         itCatEnd = list_sequence_categories_.cend();
    if ( itCatFind == itCatEnd )
        list_sequence_categories_.push_back( strCategory );
    describe( strName, desc );
    return true;
}
bool parser::value_handler(
    const char* name, const char* desc, parser::string_argument_value_handler_t fn ) {
    if ( name == nullptr || name[0] == '\0' )
        return false;
    if ( !fn )
        return false;
    skutils::string_list_t listParts = skutils::tools::split( name, chrCateoryAndNameDelimiter_ );
    size_t cntParts = listParts.size();
    if ( !( 1 <= cntParts && cntParts <= 2 ) )
        return false;
    std::string strCategory( ( cntParts == 2 ) ? listParts.front() : strDefaultCategoryName_ ),
        strName( listParts.back() );
    remove_entry( strName );
    map_opt2cat_[strName] = strCategory;
    name_sequence_list_t& cat_opts = map_category_opts_[strCategory];
    cat_opts.push_back( strName );
    value_handlers_[strName] = fn;
    list_sequence_handlers_.push_back( strName );
    name_sequence_list_t::const_iterator itCatFind = std::find( list_sequence_categories_.cbegin(),
                                             list_sequence_categories_.cend(), strCategory ),
                                         itCatEnd = list_sequence_categories_.cend();
    if ( itCatFind == itCatEnd )
        list_sequence_categories_.push_back( strCategory );
    describe( strName, desc );
    return true;
}
bool parser::remove_entry( const char* name ) {
    skutils::string_list_t listParts = skutils::tools::split( name, chrCateoryAndNameDelimiter_ );
    size_t cntParts = listParts.size();
    if ( !( 1 <= cntParts && cntParts <= 2 ) )
        return false;
    std::string /*strCategory( ( cntParts == 2 ) ? listParts.front() : strDefaultCategoryName_ ),*/
        strName( listParts.back() );
    bool bWasRemoved = false;
    if ( have_flag_handler( strName ) ) {
        bWasRemoved = true;
        flag_handlers_.erase( strName );
    }
    if ( have_value_handler( strName ) ) {
        bWasRemoved = true;
        value_handlers_.erase( strName );
    }
    std::string strCategory = map_opt2cat_[strName];
    map_opt2cat_.erase( strName );
    name_sequence_list_t& cat_opts = map_category_opts_[strCategory];
    name_sequence_list_t::iterator itNameFind =
                                       std::find( cat_opts.begin(), cat_opts.end(), strName ),
                                   itNameEnd = list_sequence_categories_.end();
    if ( itNameFind == itNameEnd )
        cat_opts.erase( itNameFind );
    if ( cat_opts.size() == 0 )
        map_category_opts_.erase( strCategory );
    return bWasRemoved;
}

static std::string stat_gen_if( const std::pair< std::string, std::string >& x ) {
    std::stringstream ss;
    static size_t g_align = 20;
    size_t nLenIfName = x.first.length(), i;
    std::string strSpace;
    if ( nLenIfName >= g_align )
        strSpace = "...";
    else {
        for ( i = nLenIfName; i < g_align; ++i )
            strSpace += '.';
    }
    ss << cc::bright( x.first ) << cc::debug( strSpace ) << cc::sunny( x.second );
    return ss.str();
}

void parser::stat_network_interfaces_info( std::ostream& os ) {
    std::list< std::pair< std::string, std::string > > lst;  // first-interface name, second-address
    std::list< std::pair< std::string, std::string > >::const_iterator itWalk, itEnd;
    lst = skutils::network::get_machine_ip_addresses( true, false );  // try IP4
    os << cc::info( "Number of of found IPv4 interfaces" ) + cc::debug( "..................." )
       << cc::size10( lst.size() ) << std::endl;
    for ( itWalk = lst.cbegin(), itEnd = lst.cend(); itWalk != itEnd; ++itWalk )
        os << cc::debug( "...." )
           << cc::info( "IPv4 interface" ) + cc::debug( "..................................." )
           << stat_gen_if( *itWalk ) << std::endl;
    lst = skutils::network::get_machine_ip_addresses( false, true );  // try IP6
    os << cc::info( "Number of of found IPv6 interfaces" ) + cc::debug( "..................." )
       << cc::size10( lst.size() ) << std::endl;
    for ( itWalk = lst.cbegin(), itEnd = lst.cend(); itWalk != itEnd; ++itWalk )
        os << cc::debug( "...." )
           << cc::info( "IPv6 interface" ) + cc::debug( "..................................." )
           << stat_gen_if( *itWalk ) << std::endl;
}

};  // namespace command_line
};  // namespace skutils
