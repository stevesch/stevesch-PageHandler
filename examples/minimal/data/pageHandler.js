// THIS IS A STOCK FILE AND SHOULD NOT NEED MODIFICATION
// Just include this script in your HTML page and trigger
// the following 2 lines as an inline script from that page:
// var reflections = [ %REFL_LIST% ];
// function initPageHandler(reflections)

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

  // source.addEventListener(name, function(e) {
  //   quietIntervals = 0;
  //   // console.log(name, e.data);
  //   document.getElementById(name).innerHTML = e.data;
  // }, false);
}

function sendValueChange(e) {
  var xhttp = new XMLHttpRequest();

  // var name = e.target.getAttribute("data-varname");
  var name = e.target.dataset.varname;
  var value = e.target.value
  var s = "/api/set?name=";
  s += escape(name);
  s += "&value=";
  s += escape(value);
  xhttp.open("GET", s, true);
  xhttp.send();
  if (e.target.dataset.clear) {
    e.target.value = "";
  }
}

function floatDef(x, xdef) {
  return x !== undefined ? parseFloat(x) : xdef;
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
      console.log("Chart x: " + datax);
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

// @param reflections: Provided by index.html or external script:
// Used to register processor reflection variables, e.g. TEMPERATURE,
// HUMIDITY, FLOATVALUE1, etc.
// This is for standard replacement of text fields within the HTML:
// (contents is dynamically populated upon send of index from server)
// var reflections = [ %REFL_LIST% ];
function initPageHandler(reflections) {
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

    // add event listeners for inputs, e.g. sliders, that
    // are not normally the sender, rather than the recipient,
    // of the value (for cases like multiple clients
    // viewing and changing values)
    // i.e. Set "watcher" class on Html elements whose value
    // should be updated when the variable specified in its
    // 'data-varname' field changes.
    document.querySelectorAll('.watcher').forEach(item => {
      var name = item.dataset.varname;
      console.log("Adding watcher for " + name);
      serverSource.addEventListener(name, function(e) {
        quietIntervals = 0;
        console.log("Watcher " + name + " = " + e.data);
        item.value = e.data;
      }, false);
    });

    document.querySelectorAll('.chartEl').forEach(item => {
      var name = item.dataset.varname;
      console.log("Adding chart for " + name);
      attachChart(item, name);
    });
  }

  setInterval(function() {
    ++quietIntervals;
    if (quietIntervals > 2) {
      // in case of unresponsive server, attempt reload-- might be updating
      quietIntervals = 0;
      location.reload();
    }
  }, 4000);
}
