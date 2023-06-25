let temperatureChart, humidityChart;

setInterval(fetchTemperature, 1000);
setInterval(fetchHumidity, 1000);

function fetchTemperature() {
    fetch('http://127.0.0.1:5000/data/temperature')
    .then(response => response.json())
    .then(data => {
      const lastestValue = data[data.length - 1].VALUE;
      document.getElementById("temperature-value").innerHTML = lastestValue + "°C";
      
      const arrValue = createArrValue(data);
      const arrTime = createArrTime(data);

      if (!temperatureChart) {
        const ctx = document.getElementById('temperature-chart').getContext('2d');
        temperatureChart = new Chart(ctx, {
          type: 'line',
          data: {
            labels: arrTime,
            datasets: [{
              label: 'Temperature value',
              data: arrValue,
              fill: true,                                     // Đặt giá trị là false để không tô màu đồ thị dưới đường vẽ. Nếu giá trị là true, đồ thị sẽ được tô màu
              backgroundColor: 'rgba(239, 71, 133, 0.2)',
              borderColor: 'rgb(239, 71, 133)',               // Màu sắc của đường đồ thị
              borderWidth: 3,                                 // Độ rộng của đường đồ thị
              pointStyle: 'circle',
              pointBackgroundColor: 'rgb(21, 41, 61)',        // Màu sắc của điểm trên đường đồ thị
              pointBorderColor: 'rgb(239, 71, 133)',          // Màu sắc của viền của điểm trên đường đồ thị
              pointHoverBackgroundColor: 'rgb(239, 71, 133)', // Màu sắc của điểm khi đưa con trỏ chuột vào trên đường đồ thị
              pointHoverBorderColor: 'rgb(239, 71, 133)',     // Màu sắc của viền khi đưa con trỏ chuột vào trên đường đồ thị
              pointRadius: 6,                                 // Kích thước của điểm trên đường đồ thị
              pointHoverRadius: 10,                            // Kích thước của điểm khi đưa con trỏ chuột vào trên đường đồ thị
              tension: 0.2                                    // Độ căng của đường đồ thị, có giá trị từ 0 đến 1. Càng gần 1 thì đường đồ thị sẽ càng mượt mà và uốn cong tự nhiên hơn
            }]
          },
          options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              legend: {
                labels: {
                  color: 'white',
                  font: {
                    family: 'Arial',
                    size: 14,
                    style: 'italic',
                    weight: 'normal',
                  },
                }
              }
            },
            animation: {
              duration: 0,
            },
            scales: {
              x: {
                title: {
                  display: true,
                  text: 'Time (hh:mm:ss)',
                  font: {
                    family: 'Arial',
                    size: 16,
                    style: 'normal',
                    weight: 'normal',
                  },
                  color: 'white',
                },
                ticks: {
                  font: {
                    family: 'Arial',
                    size: 12,
                    style: 'normal',
                    weight: 'normal',
                  },
                  color: 'white',
                  maxRotation: 180,     // Độ nghiêng tối đa cho các nhãn trên trục x
                  autoSkip: false,       // Nếu đặt là true, thì các nhãn trên trục x có thể được bỏ qua tự động nếu không cần thiết để hiển thị tất cả chúng
                  maxTicksLimit: 15,    // Giới hạn tối đa của số lượng nhãn trên trục x
                },
                grid: {
                  display: true,
                  color: 'white',
                  lineWidth: 0.1
                },      
              },
              y: {
                // min: 0,
                // max: 60,
                title: {
                  display: true,
                  text: 'Temperature (°C)',
                  font: {
                    family: 'Arial',
                    size: 16,
                    style: 'normal',
                    weight: 'normal',
                  },
                  color: 'white'
                },
                ticks: {
                  font: {
                    family: 'Arial',
                    size: 12,
                    style: 'normal',
                    weight: 'normal',
                  },
                  color: 'white',
                  autoSkip: true,
                  stepSize: 0.1,        // Khoảng cách giữa các giá trị trên trục y
                  beginAtZero: true,  // Bắt đầu từ giá trị 0 trên trục ys
                },
                grid: {
                  display: true,
                  color: 'white',
                  lineWidth: 0.1
                }, 
              }
            },
            interaction: {
              mode: 'nearest',
              intersect: false
            }
          }
        });
      } else {
        temperatureChart.data.datasets[0].data = arrValue;
        temperatureChart.data.labels = arrTime;
        temperatureChart.update();
      }
    });
}

function fetchHumidity() {
  fetch('http://127.0.0.1:5000/data/humidity')
  .then(response => response.json())
  .then(data => {
    const lastestValue = data[data.length - 1].VALUE;
    document.getElementById("humidity-value").innerHTML = lastestValue + "%";

    const arrValue = createArrValue(data);
    const arrTime = createArrTime(data);

    if (!humidityChart) {
      const ctx = document.getElementById('humidity-chart').getContext('2d');
      humidityChart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: arrTime,
          datasets: [{
            label: 'Humidity value',
            data: arrValue,
            fill: true,                                     // Đặt giá trị là false để không tô màu đồ thị dưới đường vẽ. Nếu giá trị là true, đồ thị sẽ được tô màu
            backgroundColor: 'rgba(23, 234, 137, 0.2)',
            borderColor: 'rgb(23, 234, 137)',               // Màu sắc của đường đồ thị
            borderWidth: 3,                                 // Độ rộng của đường đồ thị
            pointStyle: 'circle',
            pointBackgroundColor: 'rgb(21, 41, 61)',        // Màu sắc của điểm trên đường đồ thị
            pointBorderColor: 'rgb(23, 234, 137)',          // Màu sắc của viền của điểm trên đường đồ thị
            pointHoverBackgroundColor: 'rgb(23, 234, 137)', // Màu sắc của điểm khi đưa con trỏ chuột vào trên đường đồ thị
            pointHoverBorderColor: 'rgb(23, 234, 137)',     // Màu sắc của viền khi đưa con trỏ chuột vào trên đường đồ thị
            pointRadius: 6,                                 // Kích thước của điểm trên đường đồ thị
            pointHoverRadius: 10,                           // Kích thước của điểm khi đưa con trỏ chuột vào trên đường đồ thị
            tension: 0.2                                    // Độ căng của đường đồ thị, có giá trị từ 0 đến 1. Càng gần 1 thì đường đồ thị sẽ càng mượt mà và uốn cong tự nhiên hơn
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: {
              labels: {
                color: 'white',
                font: {
                  family: 'Arial',
                  size: 14,
                  style: 'italic',
                  weight: 'normal',
                },
              }
            }
          },
          animation: {
            duration: 0,
          },
          scales: {
            x: {
              title: {
                display: true,
                text: 'Time (hh:mm:ss)',
                font: {
                  family: 'Arial',
                  size: 16,
                  style: 'normal',
                  weight: 'normal',
                },
                color: 'white',
              },
              ticks: {
                font: {
                  family: 'Arial',
                  size: 12,
                  style: 'normal',
                  weight: 'normal',
                },
                color: 'white',
                maxRotation: 180,       // Độ nghiêng tối đa cho các nhãn trên trục x
                autoSkip: false,        // Nếu đặt là true, thì các nhãn trên trục x có thể được bỏ qua tự động nếu không cần thiết để hiển thị tất cả chúng
                maxTicksLimit: 15,      // Giới hạn tối đa của số lượng nhãn trên trục x
              },
              grid: {
                display: true,
                color: 'white',
                lineWidth: 0.1
              },      
            },
            y: {
              // min: 0,
              // max: 100,
              title: {
                display: true,
                text: 'Humidity (%)',
                font: {
                  family: 'Arial',
                  size: 16,
                  style: 'normal',
                  weight: 'normal',
                },
                color: 'white',
              },
              ticks: {
                font: {
                  family: 'Arial',
                  size: 12,
                  style: 'normal',
                  weight: 'normal',
                },
                color: 'white',
                autoSkip: true, 
                stepSize: 1,         // Khoảng cách giữa các giá trị trên trục y
                beginAtZero: true,    // Bắt đầu từ giá trị 0 trên trục y
              },
              grid: {
                display: true,
                color: 'white',
                lineWidth: 0.1
              }, 
            }
          },
          interaction: {
              mode: 'nearest',
              intersect: false
          }
        }
      });
    } else {
      humidityChart.data.datasets[0].data = arrValue;
      humidityChart.data.labels = arrTime;
      humidityChart.update();
    }
  });
}

function formatTime(datetime) {
  const hours = datetime.getHours().toString().padStart(2, '0');
  const minutes = datetime.getMinutes().toString().padStart(2, '0');
  const seconds = datetime.getSeconds().toString().padStart(2, '0');
  const timeString = hours + ':' + minutes + ':' + seconds;
  return timeString;
}
function createArrValue(data) {
  const arrValue = [];
  for (let i = 0; i < data.length; i++) {
    arrValue.push(data[i].VALUE);
  }
  return arrValue;
}
function createArrTime(data) {
  const arrTime = [];
  for (let i = 0; i < data.length; i++) {
    const datetime = new Date(data[i].TIME);
    datetime.setHours(datetime.getHours() - 7);
    arrTime.push(formatTime(datetime));
  }
  return arrTime;
}