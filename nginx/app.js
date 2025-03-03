  async function fetchData() {
  try {
    const response = await fetch("/api/data");
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
          e.textContent = data[key];
        }
      });
    });
  } catch (error) {
    console.error('Fetch error:', error);
  }
}
fetchData();
document.querySelectorAll("input[type='radio']").forEach(radio => {
  radio.addEventListener("change", (event) => {
    radio.closest("form").submit();
  });
});