// THIS IS A STOCK FILE AND SHOULD NOT NEED MODIFICATION
// Just include this script in your HTML page and trigger
// the following line as an inline script from that page:
// window.reflections = ["%REFL_LIST%"]; // NOTE: must use double-quotes

(function() {
  document.addEventListener("DOMContentLoaded", initPageHandler, false);
  
  var serverSource;
  if (!!window.EventSource) {
    serverSource = new EventSource('/events');
  } else {
    // old browser!  TODO: polyfill?
  }
  
  var quietIntervals = 0;
  
  function evElement(source, name) {
    // replace innerHTML field of elements w/data-varinner attribute matching this event
    var qstr = '[data-varinner=' + name + ']';
    var elist = document.querySelectorAll(qstr);
    source.addEventListener(name, function(e) {
      quietIntervals = 0;
      var value = e.data;
      elist.forEach(function(el) {
        el.innerHTML = value;
      });
    }, false);
  }
  
  function floatDef(x, xdef) {
    return x !== undefined ? parseFloat(x) : xdef;
  }
  
  var falses = ['0', 'false', 'undefined', 'null', ''];
  function toBool(val) {
    // handle any numeric (0, 0.0, "0", "0.0e+6")
    if (val == 0) {
      return false;
    }
    return !falses.includes(String(val));
  }
  
  function attachChart(el, varName) {
    var ctx = el.getContext("2d");
    
    var xmax = 1000 * floatDef(el.dataset.xmax, 60 * 60);
    var xinterval = 1000 * floatDef(el.dataset.xinterval, 5);
    var ymin = floatDef(el.dataset.ymin, 0);
    var ymax = floatDef(el.dataset.ymax, 255);
  
    var data = {
      labels: [],
      datasets: [
        {
          fill: 1,
          backgroundColor: "rgba(128,128,220,0.2)",
          borderColor: "rgba(128,128,220,0.1)",
          pointRadius: 0,
          pointHitRadius: 0,
          data: []
        },
        {
          label: "Data: " + varName,
          // fillColor: "rgba(220,220,220,0.2)",
          strokeColor: "rgba(0,0,0,0.75)",
          pointRadius: 0,
          pointHitRadius: 0,
          // pointColor: "rgba(220,220,220,1)",
          // pointStrokeColor: "#fff",
          // pointHighlightFill: "#fff",
          // pointHighlightStroke: "rgba(220,220,220,1)",
          fill: false,
          // backgroundColor: "rgba(128,220,128,0.2)",
          data: []
        },
        {
          fill: 1,
          backgroundColor: "rgba(220,128,128,0.2)",
          borderColor: "rgba(220,128,128,0.1)",
          pointRadius: 0,
          pointHitRadius: 0,
          data: []
        },
      ]
    };
    var ds = data.datasets;
    
    var labels = data.labels;
    var dmin = ds[0].data;
    var dataMain = ds[1].data;
    var dmax = ds[2].data;
  
    var animation = {};
    // var animation = (xinterval <= 5000) ? {
    //   duration: xinterval,
    //   easing: 'linear',
    // } : {
    //   // (defaults)
    // }
  
    var options = {
      legend: {
        display: false,
      },
      elements: {
        line: {
          tension: 0.1
        }
      },
      animation,
      scales: {
        xAxes: [
          {
            // time: {
            //   unit: 'Areas',
            // },
            gridLines: {
              // display: false,
              drawOnChartArea: false,
              drawTicks: false,
              // drawBorder: false,
            },
            ticks: {
              // maxTicksLimit: 7,
              display: false, // remove the labels on the x-axis
            },
            // 'dataset.maxBarThickness': 5,
          },
        ],
        yAxes: [
          {
            gridLines: {
              // display: false,
              // drawOnChartArea: false,
              drawTicks: false,
            },
            ticks: {
              // maxTicksLimit: 7,
              display: false, //this removed the labels on the x-axis
              min: ymin,
              max: ymax,
            },
          },
        ]
      },
    };
  
    var config = {
      type: 'line',
      data,
      options,
    };
  
    var elementChart = new Chart(ctx, config);
  
    // point-tracking:
    var ptAccum = 0;
    var ptMin = 0;
    var ptMax = 0;
    var ptCount = 0;
    var ptX0 = undefined;
  
    var doAveraging = el.dataset.average;
  
    function addDataPoint(datax, datay, ageEpsilon) {
      var dt = ageEpsilon + 1;
      var lastIndex = labels.length - 1;
      var overwrite = false;  // whether or not we should overwrite ptX0
      var tprev = ptX0;
      if (tprev) {
        dt = datax - tprev;
        overwrite = (dt < ageEpsilon);
      }
  
      var val;
  
      // overwrite e.g. if time difference from last sample is less than the ageEpsilon
      if (overwrite) {
        ptMin = Math.min(ptMin, datay);
        ptMax = Math.max(ptMax, datay);
        // note: we're assuming equal weight for all points sampled during the interval
        // (ignoring the amount of time passing between samples when averaging)
        ptAccum += datay;
        ++ptCount;
        val = doAveraging ? (ptAccum / ptCount) : datay;
  
        labels[lastIndex] = datax.getTime();
        dataMain[lastIndex] = val;
        dmin[lastIndex] = ptMin;
        dmax[lastIndex] = ptMax;
        // labels.pop();
        // dataMain.pop();
        // dmin.pop();
        // dmax.pop();
        // labels.push(datax);
        // dataMain.push(val);
        // dmin.push(ptMin);
        // dmax.push(ptMax);
      }
      else {
        ptMin = datay;
        ptMax = datay;
        ptAccum = datay;
        ptCount = 1;
        val = datay;
  
        labels.push(datax.getTime());
        dataMain.push(val);
        dmin.push(val);
        dmax.push(val);
  
        if (lastIndex >= 0) {
          ptX0 = datax;
        }
      }
  
      return true;
    }
  
  
    function removeOldData(dateNow, maxAge) {
      var changed = false;
      while (labels.length) {
        var date0 = labels[0];
        var dt = dateNow - date0;
        if (dt > maxAge) {
          labels.shift();
          dataMain.shift();
          dmin.shift();
          dmax.shift();
          changed = true;
        } else {
          break;
        }
      }
      return changed;
    }
  
  
    var idAutoSample = null;
  
    function clearAutoSample() {
      if (idAutoSample) {
        clearTimeout(idAutoSample);
        idAutoSample = null;
      }
    }
  
    function doAutoSample() {
      var lastIndex = dataMain.length - 1;
      if (lastIndex >= 0) {
        // add point
        var t = Date.now();
        var dateNow = new Date(t);
        var datay = dataMain[lastIndex];
        recordDataSample(dateNow, datay);
      }
    }
  
    function startAutoSample() {
      clearAutoSample();
      idAutoSample = setTimeout(doAutoSample, xinterval);
    }
  
    function recordDataSample(datax, datay) {
      var changed = addDataPoint(datax, datay, xinterval);
      if (removeOldData(datax, xmax)) {
        changed = true;
      }
      if (changed) {
        // console.log("Chart x: " + datax);
        // // copy to force chart update
        // data.datasets[1].data = dataMain.slice(0);
        // dataMain = data.datasets[1].data;
        // data.labels = labels.slice(0);
        // labels = data.labels;
        elementChart.update();
      }
      startAutoSample();
    }
  
    serverSource.addEventListener(varName, onDataEvent, false);
    function onDataEvent(e) {
      var t = Date.now();
      var datay = parseInt(e.data);
      var dateNow = new Date(t);  // TODO: use value from data?
      recordDataSample(dateNow, datay);
    }
  }
  
  function attachCharts() {
    document.querySelectorAll('.chartEl').forEach(item => {
      var name = item.dataset.varname;
      console.log("Adding chart for " + name);
      attachChart(item, name);
    });
  }
  
  function attachRangeTips() {
    const ranges = document.querySelectorAll("input[type='range'].tip");
    ranges.forEach((range) => {
      const bubble = document.createElement("output");
      bubble.className = "bubble";
      range.parentElement.appendChild(bubble);
      attachRangeTip(range, bubble);
    });
  
    // const allRanges = document.querySelectorAll("input[type='range']");
    // allRanges.forEach((range) => {
    //   const bubble = range.parentElement.querySelector(".bubble");
    //   if (bubble) {
    //     attachRangeTip(range, bubble);
    //   }
    // });
  
    function attachRangeTip(range, bubble) {
      console.log("Attaching range tip to " + range.dataset.varname);
      range.addEventListener("input", () => {
        setBubble(range, bubble);
      });
      setBubble(range, bubble);
    }
  
    function setBubble(range, bubble) {
      const val = range.value;
      const min = range.min ? range.min : 0;
      const max = range.max ? range.max : 100;
      const newVal = Number(((val - min) * 100) / (max - min));
      bubble.innerHTML = val;
    
      // depends on layout, size of native thumb, etc...
      // TODO: generalize
      bubble.style.left = `calc(${50 + 0.72*(newVal - 50)}%)`;
    }
  }
  
  
  // @global reflections: Provided by index.html.
  // Used to register processor reflection variables, e.g. TEMPERATURE,
  // HUMIDITY, FLOATVALUE1, etc.
  // This is for standard replacement of text fields within the HTML:
  // (contents is dynamically populated upon send of index from server)
  // window.reflections = ["%REFL_LIST%"]; // NOTE: must use double-quotes
  function initPageHandler() {
    if (!window.reflections) {
      window.reflections = [];
      console.log("WARNING, PageHandler: No reflections defined on page.");
    }
    reflections = window.reflections;
    if (reflections && reflections.length == 1 && reflections[0] === "%REFL_LIST%") {
      // server did not substitute for placeholder
      console.log("WARNING, PageHandler: reflections not replaced by server.");
      reflections.length = 0;
    }
  
    if (serverSource) {
      // serverSource.addEventListener('open', function(e) {
      //   quietIntervals = 0;
      //   // console.log("Events Connected");
      // }, false);
  
      // serverSource.addEventListener('error', function(e) {
      //   quietIntervals = 0;
      //   if (e.target.readyState != EventSource.OPEN) {
      //     // console.log("Events Disconnected");
      //   }
      // }, false);
  
      serverSource.addEventListener('message', function(e) {
        quietIntervals = 0;
        console.log("message", e.data);
      }, false);
      
      var qstr = '[data-varinner]';
      var rlist = document.querySelectorAll(qstr);
      rlist.forEach(function(el) {
        var varName = el.dataset.varinner;
        if (!reflections.includes(varName)) {
          console.log("Unregistered var (" + varName + ") added to reflection list ");
          reflections.push(varName);
        }
      });
  
      reflections.forEach(function(varName) {
        evElement(serverSource, varName);
      });
  
  
      function applyToValue(e) {
        this.value = e.data;
      }
  
      function applyToRadio(e) {
        this.checked = (e.data == item.value);
      }
  
      function applyToCheckbox(e) {
        this.checked = toBool(e.data);
      }
  
      // add event listeners for inputs, e.g. sliders, that
      // are not normally the sender, rather than the recipient,
      // of the value (for cases like multiple clients
      // viewing and changing values)
      // i.e. Set "watcher" class on Html elements whose value
      // should be updated when the variable specified in its
      // 'data-varname' field changes.
      document.querySelectorAll('.watcher').forEach(item => {
        var name = item.dataset.varname;
        var fn;
        if (item.dataset.varscript) {
          fn = new Function('event', item.dataset.varscript);
        } else {
          switch (item.type) {
            case 'checkbox':
              fn = applyToCheckbox;
              break;
            case 'radio':
              fn = applyToRadio;
              break;
            default:
            case 'range':
              fn = applyToValue;
              break;
          }
        }
        console.log("Adding watcher for " + name);
        serverSource.addEventListener(name, function(e) {
          quietIntervals = 0;
          // console.log("Watcher " + name + " = " + e.data);
          fn.call(item, e);
        }, false);
      });
  
  
      function sendSetValue(name, value) {
        var xhr = new XMLHttpRequest();
        var s = "/api/set?name=";
        s += escape(name);
        s += "&value=";
        s += escape(value);
        xhr.open("GET", s, true);
        xhr.send();
      }
  
      function sendCheckboxState(e) {
        var name = this.dataset.varname;
        var value = this.checked ? "1" : "0";
        sendSetValue(name, value);
      }
  
      function sendRadioState(e) {
        // for radio boxes, send the value of checked item(s) (ignore unchecked)
        if (this.checked) {
          var name = this.dataset.varname;
          var value = this.value;
          sendSetValue(name, value);
        }
      }
      
      function sendButtonState(e) {
        if (this.dataset.varname) {
          // send "set value" (default to 1) for specified variable
          var name = this.dataset.varname;
          var value = this.value;
          if ((value === '') || (value === undefined)) {
            value = 1;
          }
          sendSetValue(name, value);
        } else if (this.dataset.getapi) {
          // direct API call, e.g. data-getapi="/api/restart"
          var s = this.dataset.getapi;
          var target = this.dataset.target;
          var xhr = new XMLHttpRequest();
          xhr.open("GET", s, true);
          if (target) {
            switch (target) {
            case "document":
              // rewrite entire document with successful response
              xhr.onload = function () {
                document.open();
                document.write(xhr.responseText);
                document.close();
              };
              break;
            default:
              xhr.onload = function () {
                var el = document.getElementById(target);
                if (el) {
                  el.innerHTML = xhr.responseText;
                }
              };
              break;
            }
          }
          xhr.send();
        }
      }
  
      function sendValueState(e) {
        // var name = this.getAttribute("data-varname");
        var name = this.dataset.varname;
        var value = this.value
        sendSetValue(name, value);
        if (this.dataset.clear) {
          this.value = "";
        }
      }
  
      document.querySelectorAll('.sender').forEach(item => {
        var fn;
        var defEvent = 'change';
        switch (item.type) {
          case 'checkbox':
            fn = sendCheckboxState;
            break;
          case 'radio':
            fn = sendRadioState;
            break;
          case 'button':
          case 'submit':
            fn = sendButtonState;
            defEvent = 'click';
            break;
            default:
          case 'range':
            fn = sendValueState;
            break;
        }
        var name = item.dataset.varname || item.dataset.getapi;
        console.log("Adding sender for " + name);
        var handler = function (e) { fn.call(item, e); };
        if (item.dataset.sendtypes) {
          // accept 'change input' etc.
          var ta = ('' + item.dataset.sendtypes).match(/\b(\w+)\b/g);
          if (ta) {
            ta.forEach(function(tp) {
              if (tp) {
                item.addEventListener(tp, handler);
              }
            });
          }
        } else {
          // standard 'onchange' handler
          item.addEventListener(defEvent, handler);
        }
      });
  
      attachRangeTips();
      attachCharts();
    }

    var reloadCheckInterval = 4000;
    setInterval(function() {
      ++quietIntervals;
      if (quietIntervals > 2) {
        console.log("No communication from server for " + (quietIntervals*reloadCheckInterval*0.001) + " seconds");
        // in case of unresponsive server, attempt reload-- might be updating
        if (quietIntervals > 3) {
          quietIntervals = 0;
          if (true) {
            console.log("Reloading page...");
            location.reload();
          }
        }
      }
    }, reloadCheckInterval);
  }
  
  })();
  