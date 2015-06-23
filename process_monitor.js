var ps = require('ps-node');
var spawn = require('child_process').spawn;

var LOGGING = false;

var consoleLog = function( message ) {
  if ( LOGGING ) {
    console.log( message );
  }
}

// every 30 seconds, check to see wether lightrules.js is running.  if not,
var monitorProcess = function() {

  ps.lookup({
    command: 'node',

  }, function( err, resultList ) {
      if ( err ) {
        throw new Error( err );
      }

      var count = 0;

      resultList.forEach( function( process ) {
        if ( process ) {
          if ( LOGGING ) {
            console.log( 'PID: %s, COMMAND: %s, ARGUMENTS: %s', process.pid, process.command, process.arguments );
          }

          var lightProcess = /.*lightrules\.js$/

          if ( lightProcess.test( process.arguments ) ) {
            count++;
            if ( count > 1 ) {
              ps.kill( process.pid, function( err ){
                if ( err ) {
                  throw new Error( err );
                } else {
                  consoleLog( 'Duplicate process ' + process.pid + ' has been killed.' );
                }
              });
            }

          }
        }
      });

      if ( count == 0 ) {
        // restart process

        var proc = spawn('node', ['lightrules.js']);
        proc.stderr.on('data', function (data) {
          consoleLog('node lightrules.js stderr: ' + data);
        });

        proc.on('exit', function (code) {
          if (code !== 0) {
            consoleLog('node lightrules.js process exited with code ' + code);
          }
        });

        consoleLog( 'restart' );
      } else {
        consoleLog( 'lighttest.js running' );
      }
    });
};

setInterval( monitorProcess, 30000 );
