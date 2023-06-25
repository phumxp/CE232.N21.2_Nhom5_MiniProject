# from sqlalchemy.future import Connection
from flask import Flask, render_template, jsonify
from flask_mqtt import Mqtt
from flask_sqlalchemy import SQLAlchemy
from datetime import datetime
import struct

# Cấu hình kết nối tới broker MQTT
MQTT_BROKER_URL = 'mqtt.flespi.io'
MQTT_BROKER_PORT = 1883
MQTT_USERNAME = 'FlespiToken xJgIo4dRvVNp6oHvLRMT8zDvZJ0U7tsu1y1fs4ULPv1JHy2hfE0b1suP1kxWBPfs'
MQTT_CLIENT_ID = 'FLASK_CLIENT'

# Cấu hình kết nối tới database MYSQL
MYSQL_URL = 'mysql://root:123456789@localhost/mydatabase'

LIMITTED_ROW = 100
LIMITTED_DATA = 15

# Khởi tạo ứng dụng Flask
app = Flask(__name__)

# Cấu hình kết nối tới broker MQTT cho ứng dụng Flask
app.config['MQTT_BROKER_URL'] = MQTT_BROKER_URL
app.config['MQTT_BROKER_PORT'] = MQTT_BROKER_PORT
app.config['MQTT_USERNAME'] = MQTT_USERNAME
app.config['MQTT_CLIENT_ID'] = MQTT_CLIENT_ID
mqtt = Mqtt(app)

# Tạo đối tượng SQLAlchemy để kết nối và thao tác với cơ sở dữ liệu
db = SQLAlchemy()

# Đặt địa chỉ kết nối cơ sở dữ liệu MySQL
app.config['SQLALCHEMY_DATABASE_URI'] = MYSQL_URL

# Tắt theo dõi thay đổi trong SQLAlchemy để tăng tốc độ hiệu suất ứng dụng.
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

# Khởi tạo đối tượng SQLAlchemy để thực hiện các hoạt động truy vấn đến cơ sở dữ liệu.
db.init_app(app)


class TemperatureTable(db.Model):
    ID = db.Column(db.Integer, primary_key=True)
    VALUE = db.Column(db.Float)
    TIME = db.Column(db.DateTime)
class HumidityTable(db.Model):
    ID = db.Column(db.Integer, primary_key=True)
    VALUE = db.Column(db.Integer)
    TIME = db.Column(db.DateTime)
with app.app_context():
    #  Tạo ra các bảng trong cơ sở dữ liệu nếu chúng chưa tồn tại.
    db.create_all()

# Xử lý sự kiện khi đối tượng MQTT kết nối tới broker MQTT
@mqtt.on_connect()
def handle_connect(client, userdata, flags, rc):
    mqtt.subscribe('TEMPERATURE_TOPIC')
    mqtt.subscribe('HUMIDITY_TOPIC')

# Xử lý sự kiện khi có tin nhắn được gửi tới đối tượng MQTT
@mqtt.on_message()
def handle_message(client, userdata, message):
    topic = message.topic
    time = datetime.now()
    print('Time: {}'.format(time))
    if topic == 'TEMPERATURE_TOPIC':

        data = message.payload
        value = struct.unpack('f', data)[0]

        print('Received message: {}: {}'.format(topic, value))
        with app.app_context():
            # Lấy số dòng hiện có của bảng TEMPERATURE_TABLE trong cơ sở dữ liệu
            numRows = db.session.query(TemperatureTable).count()
            if numRows == LIMITTED_ROW:
                db.session.query(TemperatureTable).delete()
                db.session.commit()
                numRows = 0
            # Thêm giá trị nhiệt độ mới vào cơ sở dữ liệu
            temperature = TemperatureTable(
                ID=numRows + 1, VALUE=value, TIME=time)
            db.session.add(temperature)
            db.session.commit()

    elif topic == 'HUMIDITY_TOPIC':
        data = message.payload.decode()
        value = 0
        for element in data:
            value += ord(element)

        print('Received message: {}: {}'.format(topic, value))

        with app.app_context():
            # Lấy số dòng hiện có của bảng HUMIDITY_TABLE trong cơ sở dữ liệu
            numRows = db.session.query(HumidityTable).count()
            if numRows == LIMITTED_ROW:
                db.session.query(HumidityTable).delete()
                db.session.commit()
                numRows = 0
            # Thêm giá trị nhiệt độ mới vào cơ sở dữ liệu
            humidity = HumidityTable(ID=numRows + 1, VALUE=value, TIME=time)
            db.session.add(humidity)
            db.session.commit()

@app.route('/')
def index():
    return render_template('index.html')

def processData(data):
    result = []
    for element in data:
        dict_element = vars(element)
        del dict_element['_sa_instance_state']
        del dict_element['ID']
        result.append(dict_element)
    result.reverse()
    return result

@app.route('/data/temperature')
def temperatureData():
    temperatureData = processData(TemperatureTable.query.order_by(
        TemperatureTable.ID.desc()).limit(LIMITTED_DATA).all())
    # print('data: {}\n'.format(temperatureData))
    # Trả về JSON chứa giá trị nhiệt độ
    return jsonify(temperatureData)

@app.route('/data/humidity')
def humidityData():
    humidityData = processData(HumidityTable.query.order_by(
        HumidityTable.ID.desc()).limit(LIMITTED_DATA).all())
    # print('data: {}\n'.format(humidityData))
    # Trả về JSON chứa giá trị độ ẩm
    return jsonify(humidityData)

if __name__ == "__main__":
    # Chạy ứng dụng Flask
    app.run(debug=True)
