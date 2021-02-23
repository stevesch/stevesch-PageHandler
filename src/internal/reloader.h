#ifndef TOF_LASER_TEST_RELOADER_H_
#define TOF_LASER_TEST_RELOADER_H_

#include <pgmspace.h>   // for PROGMEM

// HTML web page to handle 3 input fields (input1, input2, input3)
const char kReloader[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html><head><title>Updating...</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
html { font-family: Verdana; }
p { font-size: 1.2em; font-weight: bold; padding: 12px 16px; box-shadow: 2px 2px 7px 2px #a0a0a0; }
</style></head>
<body>
<script>
var sp = 0;
var urlParams = new URLSearchParams(window.location.search);
var wait = parseInt(urlParams.get('wait')) || 5000;
setTimeout(function() { location.reload(); }, wait);
setInterval(function() {
document.getElementById('dots').innerHTML = Array(sp+1).join('&nbsp;') + '.';
sp = (sp + 1) % 3;
}, 500);
</script>
<p>Update in progress<span id="dots">.</span></p>
</body></html>)rawliteral";

#endif
