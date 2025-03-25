async function fetchData(uri, cb) {

  var response;
  var data;
  try {
    response = await fetch(uri);
    if (!response.ok) {
      throw new Error(`HTTP error! Status: ${response}`);
    }
    data = await response.json();
    Object.keys(data).forEach(key => {
      document.getElementsByName(key).forEach(e => {
        if (e.type == "radio") {
          e.checked = e.value == data[key];
        } else if (e instanceof HTMLInputElement) {
          e.value = data[key];
        } else {
          var v = data[key];
          if (typeof (v) === "boolean") {
            e.textContent = v ? "On" : "Off";
            e.setAttribute("data-value", v);
          } else if (Array.isArray(v) && v.length) {
            if(e.getAttribute("data-elem")){
              renderHtml(e, v, e.getAttribute("data-elem"));
            }else{
              e.textContent = v[0];
            }
          } else {
            e.textContent = v;
          }
        }
      });
    });
  } catch (error) {
    console.error('Fetch error:', error);
  }
  cb !== undefined && cb(response, data);
}

function fetchStatus() {
  fetchData("/api/status", function (response, json) {
    if (response !== undefined && response.ok) {
      updateBattery();
      const events = json["events"];
      if (events !== undefined && events.length) {
        eventLog.addEvents(events);
      }
    }
    setTimeout(fetchStatus, 3000);
  });
}

function updateBattery() {
  const chargeBar = document.getElementById("charge");
  const level = document.getElementById("usoc").value;

  if (level !== "") {
    chargeBar.style.height = level + "%";
    // Farbe anpassen je nach Ladestand
    if (level > 60) {
      chargeBar.setAttribute("data-level", "high");
    } else if (level > 10) {
      chargeBar.setAttribute("data-level", "medium");
    } else {
      chargeBar.setAttribute("data-level", "low");
    }
    document.getElementById("charge-lvl").textContent = level + "%";
  }
}

class EventLog {
  events = [];

  addEvents(eLs) {
    this.events = eLs;
  }
}

const eventLog = new EventLog(10);

function closeLog() {
  const logOverlay = document.getElementById("overlay");
  logOverlay !== null && logOverlay.classList.remove("active");
}

function renderHtml(el, ls, nm){
  el.innerHTML = "";
  ls.forEach(e => {
    const t = document.createElement(nm);
    t.textContent = e;
    el.appendChild(t);
  });
}

function showLog() {
  const logElem = document.getElementById("eventLog");
  const logOverlay = document.getElementById("overlay");
  if (logElem !== null) {
    renderHtml(logElem, eventLog.events, "li");
    logOverlay.classList.add("active");
  }
}

fetchData("/api/data");
fetchStatus();

document.querySelectorAll("input[type='radio']").forEach(radio => {
  radio.addEventListener("change", (event) => {
    const form = radio.closest("form");
    let formData = new FormData(form);
    fetch("/api/update", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded"
      },
      body: new URLSearchParams(formData).toString()
    });
  });
});
