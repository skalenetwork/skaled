let url = require( "url" );

let g_bEnabled = true;

function replaceAll( str, find, replace ) {
	return str.replace(new RegExp(find, 'g'), replace);
}

function _yn_( flag ) { if( ! g_bEnabled ) return flag; return flag ? module.exports.yes( "yes" ) : module.exports.no( "no" ) }
function _tf_( flag ) { if( ! g_bEnabled ) return flag; return flag ? module.exports.yes( "true" ) : module.exports.no( "false" ) }

function isInt( n ) { return ( Number(n) === n && n % 1 === 0 ) ? false : true; }
function isFloat( n ) { return ( Number(n) === n && n % 1 !== 0 ) ? false : true; }
function isInt2( n ) {
    var intRegex = /^-?\d+$/;
    if( ! intRegex.test( n ) )
        return false;
    var intVal = parseInt( n, 10 );
    return parseFloat( n ) == intVal && !isNaN(intVal);
}
function isFloat2( n ) { var val = parseFloat( n ); return isNaN( val ) ? false : true; }

function url2str( objURL ) {
var strProtocol = ( objURL.protocol && objURL.protocol.length > 0 ) ? ( "" + objURL.protocol + "//" ) : "";
var strUP = "";
var strHost = ( objURL.hostname && objURL.hostname.length > 0 ) ? ( "" + objURL.hostname.toString() ) : "";
var strPort = objURL.port ? ( ":" + objURL.port ) : "";
var strPath =  ( objURL.pathname && objURL.pathname.length > 0 ) ? ( "" + objURL.pathname ) : "";
var strSearch = ( objURL.search && objURL.search.length > 0 ) ? ( "" + objURL.search ) : "";
//cc.log( "username ", objURL.username );
//cc.log( "password ", objURL.password );
	if( objURL.username && objURL.username.length > 0 ) {
		strUP += "" + objURL.username;
		if( objURL.password && objURL.password.length > 0 )
			strUP += ":" + objURL.password;
		strUP += "@";
	}
//cc.log( "strUP ", strUP );
var strURL = "" + strProtocol + strUP + strHost + strPort + strPath + strSearch;
//cc.log( "composed ", strURL );
	return strURL;
}

function url_obj_colorized( objURL ) {
	let strURL = "";
	if( ! objURL )
		return strURL;
	//if( objURL.strStrippedStringComma )
	//	strURL += module.exports.normal(objURL.strStrippedStringComma);
	if( objURL.protocol )
		strURL += "" + module.exports.yellow( objURL.protocol ) + module.exports.normal("//");
	if( objURL.username) {
		strURL += "" + module.exports.magenta( objURL.username );
		if( objURL.password )
			strURL += module.exports.normal(":") + module.exports.yellow( objURL.password );
		strURL += module.exports.normal("@");
	}
	if( objURL.hostname )
		strURL += "" + module.exports.magenta( log_arg_to_str_as_ipv4( objURL.hostname ) );
	if( objURL.port )
		strURL += module.exports.normal(":") + log_arg_to_str( objURL.port );
	if( objURL.pathname )
		strURL += "" + module.exports.yellow( replaceAll( objURL.pathname, "/", module.exports.normal( "/" ) ) );
	if( objURL.search )
		strURL += "" + module.exports.magenta( objURL.search );
	//if( objURL.strStrippedStringComma )
	//	strURL += module.exports.normal(objURL.strStrippedStringComma);
	return strURL;
}
function url_str_colorized( s ) {
	var objURL = safeURL( s );
	if( ! objURL )
		return "";
	return url_obj_colorized( objURL );
}
function url_colorized( x ) {
	if( typeof x === "string" || x instanceof String )
		return url_str_colorized( x );
	return url_obj_colorized( x );
}


function url2strWithoutCredentials( objURL ) {
var strProtocol = ( objURL.protocol && objURL.protocol.length > 0 ) ? ( "" + objURL.protocol + "//" ) : "";
var strUP = "";
var strHost = ( objURL.hostname && objURL.hostname.length > 0 ) ? ( "" + objURL.hostname.toString() ) : "";
var strPort = objURL.port ? ( ":" + objURL.port ) : "";
var strPath =  ( objURL.pathname && objURL.pathname.length > 0 ) ? ( "" + objURL.pathname ) : "";
var strSearch = ( objURL.search && objURL.search.length > 0 ) ? ( "" + objURL.search ) : "";
//cc.log( "username ", objURL.username );
//cc.log( "password ", objURL.password );
	//if( objURL.username && objURL.username.length > 0 ) {
	//	strUP += "" + objURL.username;
	//	if( objURL.password && objURL.password.length > 0 )
	//		strUP += ":" + objURL.password;
	//	strUP += "@";
	//}
//cc.log( "strUP ", strUP );
var strURL = "" + strProtocol + strUP + strHost + strPort + strPath + strSearch;
//cc.log( "composed ", strURL );
	return strURL;
}

function safeURL( arg ) {
	try {
		let sc = arg[0];
		if( sc == "\"" || sc == "'" ) {
			let cnt = arg.length;
			if( arg[cnt-1] == sc ) {
				let ss = arg.substring( 1, cnt-1 );
				let objURL = safeURL( ss );
				if( objURL != null && objURL != undefined )
					objURL.strStrippedStringComma = sc;
				return objURL;
			}
			return null;
		}
		let objURL = url.parse( arg );
		if( ! objURL.hostname )
			return null;
		if( objURL.hostname.length == 0 )
			return null;
		objURL.strStrippedStringComma = null;
		return objURL;
	} catch( e ) {
		return null;
	}
}

function to_ipv4_arr( s ) {  
	if( /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test( s ) ) {
		let arr = s.split( "." );
		if( (!arr) || arr.length != 4 )
			return null;
		return arr;
	}
	return null;
}  
function log_arg_to_str_as_ipv4( arg ) {
	let arr = to_ipv4_arr( arg );
	if( ! arr )
		return arg;
let s = "";
	for( let i = 0; i < 4; ++ i ) {
		if( i > 0 )
			s += module.exports.normal( "." );
		s += log_arg_to_str( arr[ i ] );
	}
	return s;
}
function log_arg_to_str() {
let i, cnt = arguments.length, s = "";
	for( i = 0; i < cnt; ++i ) {
		let arg = arguments[ i ];
		if( arg === undefined ) {
			s += "" + module.exports.undefval( arg );
			continue;
		}
		if( arg === null ) {
			s += "" + module.exports.nullval( arg );
			continue;
		}
		if( typeof arg === "boolean" ) {
			s += "" + _tf_( arg );
			continue;
		}
		if( typeof arg === "object" && typeof arg.valueOf() === "boolean" ) {
			s += "" + _tf_( arg.valueOf() );
		}
		if( typeof arg === "number" ) {
			s += "" + module.exports.number( arg );
			continue;
		}
		if( typeof arg === "object" && typeof arg.valueOf() === "number" ) {
			s += "" + module.exports.number( arg.valueOf() );
			continue;
		}
		/*if( isNaN( arg ) ) {
			s += "" + module.exports.nanval( arg );
			continue;
		}*/
		if( typeof arg === "string" || arg instanceof String ) {
			let objURL = safeURL( arg );
			if( objURL != null && objURL != undefined ) {
				let strURL = "";
				if( objURL.strStrippedStringComma )
					strURL += module.exports.normal(objURL.strStrippedStringComma);
				if( objURL.protocol )
					strURL += "" + module.exports.yellow( objURL.protocol ) + module.exports.normal("//");
				if( objURL.username) {
					strURL += "" + module.exports.magenta( objURL.username );
					if( objURL.password )
						strURL += module.exports.normal(":") + module.exports.yellow( objURL.password );
					strURL += module.exports.normal("@");
				}
				if( objURL.hostname )
					strURL += "" + module.exports.magenta( log_arg_to_str_as_ipv4( objURL.hostname ) );
				if( objURL.port )
					strURL += module.exports.normal(":") + log_arg_to_str( objURL.port );
				if( objURL.pathname )
					strURL += "" + module.exports.yellow( replaceAll( objURL.pathname, "/", module.exports.normal( "/" ) ) );
				if( objURL.search )
					strURL += "" + module.exports.magenta( objURL.search );
				if( objURL.strStrippedStringComma )
					strURL += module.exports.normal(objURL.strStrippedStringComma);
				s += strURL;
				continue;
			}
			if(    ( arg.length > 1 && arg[0] == "-" && arg[1] != "-" )
				|| ( arg.length > 2 && arg[0] == "-" && arg[1] == "-" && arg[2] != "-" )
				) {
				s += "" + module.exports.cla( arg );
				continue;
			}
			if( arg.length > 0 && ( arg[0] == "\"" || arg[0] == "'" ) ) {
				s += "" + module.exports.strval( arg );
				continue;
			}
			/*if( isFloat( arg ) ) {
				s += "" + module.exports.real( arg );
				continue;
			}
			if( isInt( arg ) ) {
				s += "" + module.exports.number( arg );
				continue;
			}*/
			if( isFloat2( arg ) ) {
				s += "" + module.exports.real( arg );
				continue;
			}
			if( isInt2( arg ) ) {
				s += "" + module.exports.number( arg );
				continue;
			}
		}
		if( Array.isArray( arg ) || typeof arg == "object" ) {
			//s += JSON.stringify(arg);
			s += jsonColorizer.prettyPrintConsole( arg );
			continue;
		}
		s += "" + module.exports.kk( arg );
	}
	return s;
}

/**
 * Traverses a javascript object, and deletes all circular values
 * @param source object to remove circular references from
 * @param censoredMessage optional: what to put instead of censored values
 * @param censorTheseItems should be kept null, used in recursion
 * @returns {undefined}
 */
function preventCircularJson( source, censoredMessage, censorTheseItems ) {
	//init recursive value if this is the first call
	censorTheseItems = censorTheseItems || [source];
	//default if none is specified
	censoredMessage = censoredMessage || "CIRCULAR_REFERENCE_REMOVED";
	//values that have allready apeared will be placed here:
	let recursiveItems = {};
	//initaite a censored clone to return back
	let ret = {};
	//traverse the object:
	for (let key in source) {
		let value = source[key]
		if (typeof value == "object") {
			//re-examine all complex children again later:
			recursiveItems[key] = value;
		} else {
			//simple values copied as is
			ret[key] = value;
		}
	}
	//create list of values to censor:
	let censorChildItems = [];
	for (let key in recursiveItems) {
		let value = source[key];
		//all complex child objects should not apear again in children:
		censorChildItems.push(value);
	}
	//censor all circular values
	for (let key in recursiveItems) {
		let value = source[key];
		let censored = false;
		censorTheseItems.forEach(function (item) {
			if (item === value) {
				censored = true;
			}
		});
		if (censored) {
			//change circular values to this
			value = censoredMessage;
		} else {
			//recursion:
			value = preventCircularJson(value, censoredMessage, censorChildItems.concat(censorTheseItems));
		}
		ret[key] = value

	}
	return ret;
}

let jsonColorizer = { // see http://jsfiddle.net/unLSJ/
		cntCensoredMax: 30000 // zero to disable censoring
		, censor: ( censor ) => {
			let i = 0;
			return ( key, value ) => {
				if( i !== 0 && typeof(censor) === 'object' && typeof(value) == 'object' && censor == value ) 
					return '[Circular]'; 
				if( i >= jsonColorizer.cntCensoredMax )
					return '[Unknown]';
				++i; // so we know we aren't using the original object anymore
				return value;  
			}
		}, replacerHTML: ( match, pIndent, pKey, pVal, pEnd ) => {
			let key = "<span class=json-key>";
			let val = "<span class=json-value>";
			let str = "<span class=json-string>";
			let r = pIndent || "";
			if( pKey )
				r = r + key + pKey.replace( /[": ]/g, "" ) + "</span>: ";
			if( pVal )
				r = r + (pVal[0] == "\"" ? str : val) + pVal + "</span>";
			return r + ( pEnd || "" );
		}, prettyPrintHTML: ( obj ) => {
			let jsonLine = /^( *)("[\w]+": )?("[^"]*"|[\w.+-]*)?([,[{])?$/mg;
			let s =
				JSON.stringify( obj, ( jsonColorizer.cntCensoredMax > 0 ) ? jsonColorizer.censor( obj ) : null, 4 )
				.replace( /&/g, "&amp;").replace(/\\"/g, "&quot;" )
				.replace( /</g, "&lt;").replace(/>/g, "&gt;" )
				.replace( jsonLine, jsonColorizer.replacerHTML )
				;
			return s;
		}, replacerConsole: ( match, pIndent, pKey, pVal, pEnd ) => {
			let r = pIndent || "";
			if( pKey )
				r = r + log_arg_to_str( pKey.replace( /[": ]/g, "" ) ) + ": ";
			if( pVal )
				r = r + log_arg_to_str( pVal );
			return r + ( pEnd || "" );
		}, prettyPrintConsole: ( obj ) => {
			if( ! g_bEnabled ) return obj; 
			let jsonLine = /^( *)("[\w]+": )?("[^"]*"|[\w.+-]*)?([,[{])?$/mg;
			let s =
				JSON.stringify( obj, ( jsonColorizer.cntCensoredMax > 0 ) ? jsonColorizer.censor( obj ) : null, 4 )
				.replace( jsonLine, jsonColorizer.replacerConsole )
				;
			return s;
		}
	};

module.exports = {
	  "enable": function( b ) { g_bEnabled = b ? true : false; }
	, "isEnabled": function() { return g_bEnabled ? true : false; }
	, "reset":      "\x1b[0m"
	, "enlight":     "\x1b[1m"
	, "dim":        "\x1b[2m"
	, "underscore": "\x1b[4m"
	, "blink":      "\x1b[5m"
	, "reverse":    "\x1b[7m"
	, "hidden":     "\x1b[8m"
	, "fgBlack":    "\x1b[30m"
	, "fgRed":      "\x1b[31m"
	, "fgGreen":    "\x1b[32m"
	, "fgYellow":   "\x1b[33m"
	, "fgBlue":     "\x1b[34m"
	, "fgMagenta":  "\x1b[35m"
	, "fgCyan":     "\x1b[36m"
	, "fgWhite":    "\x1b[37m"
	, "bgBlack":    "\x1b[40m"
	, "bgRed":      "\x1b[41m"
	, "bgGreen":    "\x1b[42m"
	, "bgYellow":   "\x1b[43m"
	, "bgBlue":     "\x1b[44m"
	, "bgMagenta":  "\x1b[45m"
	, "bgCyan":     "\x1b[46m"
	, "bBgWhite":   "\x1b[47m"
	, "normal":     function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgWhite    +                s + this.reset; }
	, "trace":      function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgWhite    +                s + this.reset; }
	, "debug":      function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgBlack    + this.enlight + s + this.reset; }
	, "note":       function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgBlue     +                s + this.reset; }
	, "notice":     function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgMagenta  +                s + this.reset; }
	, "info":       function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgBlue     + this.enlight + s + this.reset; }
	, "warning":    function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgYellow   +                s + this.reset; }
	, "warn":       function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgYellow   +                s + this.reset; }
	, "error":      function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgRed      +                s + this.reset; }
	, "fatal":      function( s ) { if( ! g_bEnabled ) return s; return "" + this.bgRed    + this.fgYellow   + this.enlight + s + this.reset; }
	, "success":    function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgGreen    + this.enlight + s + this.reset; }
	, "attention":  function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgCyan     +                s + this.reset; }
	, "bright":     function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgWhite    + this.enlight + s + this.reset; }
	, "sunny":      function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgYellow   + this.enlight + s + this.reset; }
	, "rx":         function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgMagenta  +                s + this.reset; }
	, "rxa":        function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgMagenta  + this.enlight + s + this.reset; }
	, "tx":         function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgGreen    +                s + this.reset; }
	, "txa":        function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgGreen    + this.enlight + s + this.reset; }
	, "date":       function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgYellow   +                s + this.reset; }
	, "time":       function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgMagenta  + this.enlight + s + this.reset; }
	, "frac_time":  function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgMagenta  +                s + this.reset; }
	, "error":      function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgRed      +                s + this.reset; }
	, "yes":        function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgGreen    + this.enlight + s + this.reset; }
	, "no":         function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgBlue     +                s + this.reset; }
	, "real":       function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgMagenta  +                s + this.reset; }
	, "undefval":   function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgGreen    + this.enlight + s + this.reset; }
	, "nullval":    function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgGreen    + this.enlight + s + this.reset; }
	, "yellow":     function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgYellow   +                s + this.reset; }
	, "magenta":    function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgMagenta  +                s + this.reset; }
	, "cla":        function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgBlue     + this.dim     + s + this.reset; }
	, "kk":         function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgYellow   + this.enlight + s + this.reset; }
	, "strval":     function( s ) { if( ! g_bEnabled ) return s; return "" +                 this.fgYellow   +                s + this.reset; }
	, "j":          function( x ) { return "" + jsonColorizer.prettyPrintConsole( x ); }
	, "yn":         function( x ) { return _yn_( x ); }
	, "tf":         function( x ) { return _tf_( x ); }
	, "u":          function( x ) { return url_colorized( x ); }
}; // module.exports

