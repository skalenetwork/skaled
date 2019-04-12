const cc = require( "./cc.js" );
const fs = require( "fs" );
const C = require( "constants" );
let g_arrStreams = [ ];

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function n2s( n, sz ) {
let s = "" + n;
	while( s.length < sz )
		s = "0" + s;
	return s;
}

function generateTimestamp() {
let ts = new Date();
let s =
	""               + cc.date(      n2s( ts.getUTCFullYear(),     4 ) )
	+ cc.bright("-") + cc.date(      n2s( ts.getUTCMonth(),        2 ) )
	+ cc.bright("-") + cc.date(      n2s( ts.getUTCDate(),         2 ) )
	+ " "            + cc.time(      n2s( ts.getUTCHours(),        2 ) )
	+ cc.bright(":") + cc.time(      n2s( ts.getUTCMinutes(),      2 ) )
	+ cc.bright(":") + cc.time(      n2s( ts.getUTCSeconds(),      2 ) )
	+ cc.bright(".") + cc.frac_time( n2s( ts.getUTCMilliseconds(), 3 ) )
	;
	return s;
}
function generateTimestampPrefix() {
	return generateTimestamp() + cc.bright(":") + " ";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function removeAllStreams() {
let i = 0, cnt = 0;
	try { 
		cnt = g_arrStreams.length;
		for( i = 0; i < cnt; ++ i ) {
			try {
				let objEntry = g_arrStreams[ i ];
				objEntry.objStream.close();
			} catch( e ) {
			}
		}
	} catch( e ) {
	}
	g_arrStreams = [];
}

function getStreamWithFilePath( strFilePath ) {
	try { 
		cnt = g_arrStreams.length;
		for( i = 0; i < cnt; ++ i ) {
			try {
				let objEntry = g_arrStreams[ i ];
				if( objEntry.strPath === strFilePath )
					return objEntry;
			} catch( e ) {
			}
		}
	} catch( e ) {
	}
	return null;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function createStandardOutputStream() {
	try {
		let objEntry = {
			"strPath": "stdout"
			, "nMaxSizeBeforeRotation": -1
			, "nMaxFilesCount": -1
			, "objStream": null
			, "write": function( s ) { let x = "" + s; try { if( this.objStream ) this.objStream.write( x ); } catch( e ) { } }
			, "close": function() { this.objStream = null; }
			, "open": function() { try { this.objStream = process.stdout; } catch( e ) { } }
			, "size": function() { return 0; }
			, "rotate": function( nBytesToWrite ) { }
		};
		objEntry.open();
		return objEntry;
	} catch( e ) {
	}
	return null;
}

function insertStandardOutputStream() {
let objEntry = getStreamWithFilePath( "stdout" );
	if( objEntry !== null )
		return true;
	objEntry = createStandardOutputStream();
	if( ! objEntry )
		return false;
	g_arrStreams.push( objEntry );
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function createFileOutput( strFilePath, nMaxSizeBeforeRotation, nMaxFilesCount ) {
	try {
		let fd = fs.openSync( "" + strFilePath, "a", C.O_NONBLOCK|C.O_WR );
		let objEntry = {
			  "strPath": "" + strFilePath
			, "nMaxSizeBeforeRotation": 0 + nMaxSizeBeforeRotation
			, "nMaxFilesCount": 0 + nMaxFilesCount
			, "objStream": null
			, "write": function( s ) { let x = "" + s; this.rotate( x.length ); fs.appendFileSync( this.objStream, x, "utf8" ); }
			, "close": function() { if( ! this.objStream ) return; fs.closeSync( this.objStream ); this.objStream = null; }
			, "open": function() { this.objStream = fs.openSync( this.strPath, "a", C.O_NONBLOCK|C.O_WR ); }
			, "size": function() { try { return fs.lstatSync( this.strPath ).size; } catch( e ) { return 0; } }
			, "rotate": function( nBytesToWrite ) {
				try {
					if( this.nMaxSizeBeforeRotation <= 0 || this.nMaxFilesCount <= 1 )
						return;
					this.close();
					const nFileSize = this.size();
					const nNextSize = nFileSize + nBytesToWrite;
					if( nNextSize <= this.nMaxSizeBeforeRotation ) {
						this.open();
						return;
					}
					let i = 0, cnt = 0 + this.nMaxFilesCount;
					for( i = 0; i < cnt; ++ i ) {
						let j = this.nMaxFilesCount - i - 1;
						let strPath = "" + this.strPath + ( ( j == 0 ) ? "" : ( "." + j ) );
						if( j == ( cnt - 1 ) ) {
							try { fs.unlinkSync( strPath ); } catch( e ) { }
							continue;
						}
						let strPathPrev = "" + this.strPath +  "." + ( j + 1 );
						try { fs.unlinkSync( strPathPrev ); } catch( e ) { }
						try { fs.renameSync( strPath, strPathPrev ); } catch( e ) { }
					} // for( i = 0; i < cnt; ++ i )
				} catch( e ) {
				}
				try {
					this.open();
				} catch( e ) {
				}
			}
		};
		objEntry.open();
		return objEntry;
	} catch( e ) {
		console.log( "CRITICAL ERROR: Failed to open file system log stream for " + strFilePath );
	}
	return null;
}
function insertFileOutput( strFilePath, nMaxSizeBeforeRotation, nMaxFilesCount ) {
let objEntry = getStreamWithFilePath( "" + strFilePath );
	if( objEntry !== null )
		return true;
	objEntry = createFileOutput( strFilePath, nMaxSizeBeforeRotation, nMaxFilesCount );
	if( ! objEntry )
		return false;
	g_arrStreams.push( objEntry );
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

module.exports = {
	"write": function() {
		let s = generateTimestampPrefix(), i = 0, cnt = 0;
		try { 
			cnt = arguments.length;
			for( i = 0; i < cnt; ++ i ) {
				try {
					s += arguments[ i ];
				} catch( e ) {
				}
			}
		} catch( e ) {
		}
		try { 
			if( s.length <= 0 )
				return;
			cnt = g_arrStreams.length;
			for( i = 0; i < cnt; ++ i ) {
				try {
					let objEntry = g_arrStreams[ i ];
					objEntry.write( s );
				} catch( e ) {
				}
			}
		} catch( e ) {
		}
	}, "removeAll": function() {
		removeAllStreams();
	}, "addStdout": function() {
		return insertStandardOutputStream();
	}, "add": function( strFilePath, nMaxSizeBeforeRotation, nMaxFilesCount ) {
		return insertFileOutput(
			strFilePath,
			( nMaxSizeBeforeRotation <= 0 ) ? -1 : nMaxSizeBeforeRotation,
			( nMaxFilesCount <= 1 ) ? -1 : nMaxFilesCount
			);
	}
}; // module.exports

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

