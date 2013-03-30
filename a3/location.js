var http = require('http');

var chatserver = 'localhost 8000 8000';

http.createServer(function(req, res) {
  switch(req.url) {
    case '/~csc469h/winter/chatserver.txt':
      res.end(chatserver);
      break;

    default:
      res.statusCode = 404;
      res.end();
  }
  console.log('Request: ' + req.url);
  console.log('Response: ' + res.statusCode);
}).listen(80);

console.log('Location server listening on Port 80');
