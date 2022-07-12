console.log("hello world!");

const counter = document.getElementById("counter");

window.setInterval(() => counter.innerText = parseInt(counter.innerText) + 1, 1000);
