var initialized = false;
var options = {};

Pebble.addEventListener("ready", function() {
  console.log("ready called!");
  initialized = true;
});

Pebble.addEventListener("showConfiguration", function() {
  console.log("showing configuration");
  Pebble.openURL('http://blog.uptodown.com/ajax/configurable.html?'+encodeURIComponent(JSON.stringify(options)));
  console.log('http://blog.uptodown.com/ajax/configurable.html?'+encodeURIComponent(JSON.stringify(options)));
});

Pebble.addEventListener("webviewclosed", function(e) {
  console.log("configuration closed");
  // webview closed
  //Using primitive JSON validity and non-empty check
  if (e.response.charAt(0) == "{" && e.response.slice(-1) == "}" && e.response.length > 5) {
    options = JSON.parse(decodeURIComponent(e.response));
    console.log("Options = " + JSON.stringify(options));
    
    localStorage.setItem('options', JSON.stringify(options));
	  
    
    var responseFromWebView = decodeURIComponent(e.response);
    var settings = JSON.parse(responseFromWebView);
    
    
    Pebble.sendAppMessage(settings,
        function(e) {
          console.log("Enviado config");
        },
        function(e) {
          console.log("Error sending info to Pebble!");
        });
    
    
  } else {
    console.log("Cancelled");
  }
});
