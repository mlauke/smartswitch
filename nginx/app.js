  async function fetchData(uri) {
  try {
    const response = await fetch(uri);
    if (!response.ok) {
      throw new Error(`HTTP error! Status: ${response}`);
    }
    const data = await response.json();
    Object.keys(data).forEach(key => {
      document.getElementsByName(key).forEach(e => {
        if(e.type == "radio"){
          e.checked = e.value == data[key];
        }else if(e instanceof HTMLInputElement) {
          e.value = data[key];
        }else {
          var v = data[key];
          if(typeof(v) == "boolean"){
            e.textContent = v ? "On" : "Off";
          }else{
            e.textContent = v;
          }
        }
      });
    });
  } catch (error) {
    console.error('Fetch error:', error);
  }
}

function fetchStatus() {
  fetchData("/api/status");
  setTimeout(fetchStatus, 3000);
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