var ps = require('ps-node');
var childProcess = require("child_process");
var spawn = childProcess.spawn;

var fs = require('fs'),
  out = fs.openSync('./out.log', 'a'),
  err = fs.openSync('./out.log', 'a');

var LOGGING = false;

var count = 0;

var consoleLog = function( message ) {
  if ( LOGGING ) {
    console.log( message );
  }
}
var initialize = function() {
  var oldSpawn = childProcess.spawn;
  function mySpawn() {
      console.log('spawn called');
      console.log(arguments);
      var result = oldSpawn.apply(this, arguments);
      return result;
  }
  childProcess.spawn = mySpawn;
  spawn = mySpawn;
}

// every 30 seconds, check to see wether lightrules.js is running.  if not,
var monitorProcess = function() {

  count = 0;

  ps.lookup({
    command: 'node',
    psargs: 'aux'
  }, function( err, resultList ) {
      if ( err ) {
        throw new Error( err );
      }
      if ( LOGGING ){
        console.log( 'found %s processes', resultList.length );
      }

      resultList.forEach( function( process ) {
        if ( process ) {
          if ( LOGGING ) {
            console.log( 'PID: %s, COMMAND: %s, ARGUMENTS: %s', process.pid, process.command, process.arguments );
          }
          var lightProcess = /.*lightrules\.js$/

          if ( lightProcess.test( process.arguments ) ) {
            count++;
            /*
            if ( count > 1 ) {
              ps.kill( process.pid, function( err ){
                if ( err ) {
                  throw new Error( err );
                } else {
                  consoleLog( 'Duplicate process ' + process.pid + ' has been killed.' );
                }
              });
            }
            */

          }
        }
      });

      if ( count == 1 ) {
        consoleLog( 'lightrules.js is running' );
      } else {
        consoleLog( 'calling restartProcess() ');
        restartProcess();
      }

/*
      if ( count == 0 ) {
        // restart process
        createProcess();
        consoleLog( 'restart' );
      } else {
        consoleLog( 'lightrules.js running' );
      }
*/
    });
};

var createProcess = function(){
  var proc = spawn('node', ['lightrules.js'], {
    //detached: true,
    stdio: [ 'ignore', out, err ]
  });

  //proc.unref();
}

var restartProcess = function(){
  consoleLog( 'stopping Lights' );

  var stop = spawn('/home/pi/sequential-environment/stop-lights',[], {
    // detached: true,
    stdio: ['ignore', out, err ]
  });

  setTimeout( function(){
    consoleLog( 'restarting Lights' );

    var proc = spawn('/home/pi/sequential-environment/start-lights',[], {
      // detached: true,
      stdio: ['ignore', out, err ]
    });
  }, 5000 );
}

//initialize();
setInterval( monitorProcess, 30000 );
