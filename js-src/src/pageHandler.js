// THIS IS A STOCK FILE AND SHOULD NOT NEED MODIFICATION
// Just include this script in your HTML page and trigger
// the following line as an inline script from that page:
// window.reflections = ["%REFL_LIST%"]; // NOTE: must use double-quotes

(() => {
  // eslint-disable-next-line no-use-before-define
  document.addEventListener('DOMContentLoaded', initPageHandler, false);

  let serverSource;
  if (window.EventSource) {
    serverSource = new EventSource('/events');
  } else {
    // old browser!  TODO: polyfill?
  }
  const varSource = new EventTarget();

  let quietIntervals = 0;

  // msgBlock is semicolon-separated strings of var/value messages.
  // Each strKv is a comma-separated pair of base64-encoded strings,
  // with the [0] being the variable name, and [1] being the new variable value.
  function processVarsMessage(msgBlock) {
    const strKvPairs = msgBlock.split(';');
    strKvPairs.forEach((strKv) => {
      const kv = strKv.split(',');
      const varName = atob(kv[0]);
      const varValue = atob(kv[1]);
      const msg = new MessageEvent(varName, { data: varValue });
      varSource.dispatchEvent(msg);
    });
  }

  function handleVarsMessage(e) {
    processVarsMessage(e.data);
  }

  function evElement(source, name) {
    // replace innerHTML field of elements w/data-varinner attribute matching this event
    const qstr = `[data-varinner=${name}]`;
    const elist = document.querySelectorAll(qstr);
    source.addEventListener(name, (e) => {
      quietIntervals = 0;
      const value = e.data;
      elist.forEach((el) => {
        el.innerHTML = value;
      });
    }, false);
  }

  function floatDef(x, xdef) {
    return x !== undefined ? parseFloat(x) : xdef;
  }

  const falses = ['0', 'false', 'undefined', 'null', ''];
  function toBool(val) {
    // handle any numeric (0, 0.0, "0", "0.0e+6")
    // eslint-disable-next-line eqeqeq
    if (val == 0) {
      return false;
    }
    return !falses.includes(String(val));
  }

  function attachChart(el, varName) {
    const ctx = el.getContext('2d');

    const xmax = 1000 * floatDef(el.dataset.xmax, 60 * 60);
    const xinterval = 1000 * floatDef(el.dataset.xinterval, 5);
    const ymin = floatDef(el.dataset.ymin, 0);
    const ymax = floatDef(el.dataset.ymax, 255);

    const data = {
      labels: [],
      datasets: [
        {
          fill: 1,
          backgroundColor: 'rgba(128,128,220,0.2)',
          borderColor: 'rgba(128,128,220,0.1)',
          pointRadius: 0,
          pointHitRadius: 0,
          data: [],
        },
        {
          label: `Data: ${varName}`,
          // fillColor: "rgba(220,220,220,0.2)",
          strokeColor: 'rgba(0,0,0,0.75)',
          pointRadius: 0,
          pointHitRadius: 0,
          // pointColor: "rgba(220,220,220,1)",
          // pointStrokeColor: "#fff",
          // pointHighlightFill: "#fff",
          // pointHighlightStroke: "rgba(220,220,220,1)",
          fill: false,
          // backgroundColor: "rgba(128,220,128,0.2)",
          data: [],
        },
        {
          fill: 1,
          backgroundColor: 'rgba(220,128,128,0.2)',
          borderColor: 'rgba(220,128,128,0.1)',
          pointRadius: 0,
          pointHitRadius: 0,
          data: [],
        },
      ],
    };
    const ds = data.datasets;

    const { labels } = data;
    const dmin = ds[0].data;
    const dataMain = ds[1].data;
    const dmax = ds[2].data;

    const animation = {};
    // var animation = (xinterval <= 5000) ? {
    //   duration: xinterval,
    //   easing: 'linear',
    // } : {
    //   // (defaults)
    // }

    const options = {
      legend: {
        display: false,
      },
      elements: {
        line: {
          tension: 0.1,
        },
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
              display: false, // this removed the labels on the x-axis
              min: ymin,
              max: ymax,
            },
          },
        ],
      },
    };

    const config = {
      type: 'line',
      data,
      options,
    };

    // Note: requires script in html:
    // <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.4/Chart.min.js" integrity="sha512-d9xgZrVZpmmQlfonhQUvTR7lMPtO7NkZMkA0ABN3PHCbKA5nqylQ/yWlFAyY6hYgdF1Qh6nYiuADWwKB4C2WSw==" crossorigin="anonymous"></script>
    // eslint-disable-next-line no-undef
    const elementChart = new Chart(ctx, config);

    // point-tracking:
    let ptAccum = 0;
    let ptMin = 0;
    let ptMax = 0;
    let ptCount = 0;
    let ptX0;

    const doAveraging = el.dataset.average;

    function addDataPoint(datax, datay, ageEpsilon) {
      let dt = ageEpsilon + 1;
      const lastIndex = labels.length - 1;
      let overwrite = false; // whether or not we should overwrite ptX0
      const tprev = ptX0;
      if (tprev) {
        dt = datax - tprev;
        overwrite = (dt < ageEpsilon);
      }

      let val;

      // overwrite e.g. if time difference from last sample is less than the ageEpsilon
      if (overwrite) {
        ptMin = Math.min(ptMin, datay);
        ptMax = Math.max(ptMax, datay);
        // note: we're assuming equal weight for all points sampled during the interval
        // (ignoring the amount of time passing between samples when averaging)
        ptAccum += datay;
        ptCount += 1;
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
      } else {
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
      let changed = false;
      while (labels.length) {
        const date0 = labels[0];
        const dt = dateNow - date0;
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

    let idAutoSample = null;

    function clearAutoSample() {
      if (idAutoSample) {
        clearTimeout(idAutoSample);
        idAutoSample = null;
      }
    }

    function startAutoSample() {
      clearAutoSample();
      // eslint-disable-next-line no-use-before-define
      idAutoSample = setTimeout(doAutoSample, xinterval);
    }

    function recordDataSample(datax, datay) {
      let changed = addDataPoint(datax, datay, xinterval);
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

    function doAutoSample() {
      const lastIndex = dataMain.length - 1;
      if (lastIndex >= 0) {
        // add point
        const t = Date.now();
        const dateNow = new Date(t);
        const datay = dataMain[lastIndex];
        recordDataSample(dateNow, datay);
      }
    }

    function onDataEvent(e) {
      const t = Date.now();
      const datay = parseInt(e.data, 10);
      const dateNow = new Date(t); // TODO: use value from data?
      recordDataSample(dateNow, datay);
    }
    varSource.addEventListener(varName, onDataEvent, false);
  }

  function attachCharts() {
    document.querySelectorAll('.chartEl').forEach((item) => {
      const name = item.dataset.varname;
      console.log(`Adding chart for ${name}`);
      attachChart(item, name);
    });
  }

  function setBubble(range, bubble) {
    const val = range.value;
    const min = range.min ? range.min : 0;
    const max = range.max ? range.max : 100;
    const newVal = Number(((val - min) * 100) / (max - min));
    bubble.innerHTML = val;

    // depends on layout, size of native thumb, etc...
    // TODO: generalize
    bubble.style.left = `calc(${50 + 0.72 * (newVal - 50)}%)`;
  }

  function attachRangeTip(range, bubble) {
    console.log(`Attaching range tip to ${range.dataset.varname}`);
    range.addEventListener('input', () => {
      setBubble(range, bubble);
    });
    setBubble(range, bubble);
  }

  function attachRangeTips() {
    const ranges = document.querySelectorAll("input[type='range'].tip");

    ranges.forEach((range) => {
      const bubble = document.createElement('output');
      bubble.className = 'bubble';
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
  }

  function applyToValue(e) {
    this.value = e.data;
  }

  function applyToRadio(e) {
    this.checked = (e.data == this.value); // eslint-disable-line eqeqeq
  }

  function applyToCheckbox(e) {
    this.checked = toBool(e.data);
  }

  function getAllReceived(e) {
    const msgBlock = e.currentTarget.response;
    // console.log(`### all >>> ${msgBlock}`);
    processVarsMessage(msgBlock);
  }

  // @global reflections: Provided by index.html.
  // Used to register processor reflection variables, e.g. TEMPERATURE,
  // HUMIDITY, FLOATVALUE1, etc.
  // This is for standard replacement of text fields within the HTML:
  // (contents is dynamically populated upon send of index from server)
  // window.reflections = ["%REFL_LIST%"]; // NOTE: must use double-quotes
  function initPageHandler() {
    function requestAllValues() {
      const xhr = new XMLHttpRequest();
      const s = '/api/getall';
      xhr.addEventListener('load', getAllReceived, false);
      // xhr.onload = ;
      xhr.open('GET', s, true);
      xhr.send();
    }

    function sendSetValue(name, value) {
      const xhr = new XMLHttpRequest();
      let s = '/api/set?name=';
      s += escape(name);
      s += '&value=';
      s += escape(value);
      xhr.open('GET', s, true);
      xhr.send();
    }

    function sendCheckboxState(/* e */) {
      const name = this.dataset.varname;
      const value = this.checked ? '1' : '0';
      sendSetValue(name, value);
    }

    function sendRadioState(/* e */) {
      // for radio boxes, send the value of checked item(s) (ignore unchecked)
      if (this.checked) {
        const name = this.dataset.varname;
        const { value } = this;
        sendSetValue(name, value);
      }
    }

    function sendButtonState(/* e */) {
      if (this.dataset.varname) {
        // send "set value" (default to 1) for specified variable
        const name = this.dataset.varname;
        let { value } = this;
        if ((value === '') || (value === undefined)) {
          value = 1;
        }
        sendSetValue(name, value);
      } else if (this.dataset.getapi) {
        // direct API call, e.g. data-getapi="/api/restart"
        const s = this.dataset.getapi;
        const { target } = this.dataset;
        const xhr = new XMLHttpRequest();
        xhr.open('GET', s, true);
        if (target) {
          switch (target) {
            case 'document':
              // rewrite entire document with successful response
              xhr.onload = () => {
                document.open();
                document.write(xhr.responseText);
                document.close();
              };
              break;
            default:
              xhr.onload = () => {
                const el = document.getElementById(target);
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

    function sendValueState(/* e */) {
      // var name = this.getAttribute("data-varname");
      const name = this.dataset.varname;
      const { value } = this;
      sendSetValue(name, value);
      if (this.dataset.clear) {
        this.value = '';
      }
    }

    if (!window.reflections) {
      window.reflections = [];
      console.log('WARNING, PageHandler: No reflections defined on page.');
    }
    const refls = window.reflections;
    if (refls && refls.length === 1 && refls[0] === '%REFL_LIST%') {
      // server did not substitute for placeholder
      console.log('WARNING, PageHandler: reflections not replaced by server.');
      refls.length = 0;
    }

    if (serverSource) {
      // serverSource.addEventListener('open', function(/* e */) {
      //   quietIntervals = 0;
      //   // console.log("Events Connected");
      // }, false);

      // serverSource.addEventListener('error', function(e) {
      //   quietIntervals = 0;
      //   if (e.target.readyState != EventSource.OPEN) {
      //     // console.log("Events Disconnected");
      //   }
      // }, false);

      serverSource.addEventListener('_M_', handleVarsMessage);

      serverSource.addEventListener('message', (e) => {
        quietIntervals = 0;
        console.log('message', e.data);
      }, false);

      const qstr = '[data-varinner]';
      const rlist = document.querySelectorAll(qstr);
      rlist.forEach((el) => {
        const varName = el.dataset.varinner;
        if (!refls.includes(varName)) {
          console.log(`Unregistered var (${varName}) added to reflection list `);
          refls.push(varName);
        }
      });

      refls.forEach((varName) => {
        evElement(varSource, varName);
      });

      // add event listeners for inputs, e.g. sliders, that
      // are not normally the sender, rather than the recipient,
      // of the value (for cases like multiple clients
      // viewing and changing values)
      // i.e. Set "watcher" class on Html elements whose value
      // should be updated when the variable specified in its
      // 'data-varname' field changes.
      document.querySelectorAll('.watcher').forEach((item) => {
        const name = item.dataset.varname;
        let fn;
        if (item.dataset.varscript) {
          // eslint-disable-next-line no-new-func
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
        console.log(`Adding watcher for ${name}`);
        varSource.addEventListener(name, (e) => {
          quietIntervals = 0;
          // console.log("Watcher " + name + " = " + e.data);
          fn.call(item, e);
        }, false);
      });

      document.querySelectorAll('.sender').forEach((item) => {
        let fn;
        let defEvent = 'change';
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
        const name = item.dataset.varname || item.dataset.getapi;
        console.log(`Adding sender for ${name}`);
        const handler = (e) => fn.call(item, e);
        if (item.dataset.sendtypes) {
          // accept 'change input' etc.
          const ta = (`${item.dataset.sendtypes}`).match(/\b(\w+)\b/g);
          if (ta) {
            ta.forEach((tp) => {
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

    const reloadCheckInterval = 4000;
    setInterval(() => {
      quietIntervals += 1;
      if (quietIntervals > 2) {
        console.log(`No communication from server for ${quietIntervals * reloadCheckInterval * 0.001} seconds`);
        // in case of unresponsive server, attempt reload-- might be updating
        if (quietIntervals > 3) {
          quietIntervals = 0;

          console.log('Reloading page...');
          window.location.reload();
        }
      }
    }, reloadCheckInterval);

    requestAllValues();
  }
})();
