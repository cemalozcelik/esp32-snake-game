import sys
import serial
import serial.tools.list_ports
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                               QHBoxLayout, QPushButton, QLabel, QComboBox, 
                               QTextEdit, QGridLayout)
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QFont, QKeyEvent, QIcon

class SnakeGameController(QMainWindow):
    def __init__(self):
        super().__init__()
        self.serial_port = None
        self.init_ui()
        
    def init_ui(self):
        self.setWindowTitle("ESP32 Snake Game Controller")
        self.setGeometry(100, 100, 600, 500)
        self.setWindowIcon(QIcon("esp32-snake-game/serial-version/python-controller/assets/snake_icon.ico"))
        self.setStyleSheet("""
            QMainWindow {
                background-color: #1e1e1e;
            }
            QLabel {
                color: #00ff00;
                font-size: 14px;
            }
            QPushButton {
                background-color: #2d2d2d;
                color: #00ff00;
                border: 2px solid #00ff00;
                border-radius: 5px;
                padding: 10px;
                font-size: 16px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #00ff00;
                color: #000000;
            }
            QPushButton:pressed {
                background-color: #00aa00;
            }
            QComboBox {
                background-color: #2d2d2d;
                color: #00ff00;
                border: 2px solid #00ff00;
                padding: 5px;
                font-size: 14px;
            }
            QTextEdit {
                background-color: #0d0d0d;
                color: #00ff00;
                border: 2px solid #00ff00;
                font-family: 'Courier New';
                font-size: 12px;
            }
        """)
        
        # Ana widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # Başlık
        title = QLabel("🐍 ESP32 Snake Game Controller 🐍")
        title.setAlignment(Qt.AlignCenter)
        title.setFont(QFont("Arial", 24, QFont.Bold))
        main_layout.addWidget(title)
        
        # Port seçimi
        port_layout = QHBoxLayout()
        port_label = QLabel("Serial Port:")
        self.port_combo = QComboBox()
        self.refresh_ports()
        refresh_btn = QPushButton("🔄 Refresh")
        refresh_btn.clicked.connect(self.refresh_ports)
        self.connect_btn = QPushButton("🔌 Connect")
        self.connect_btn.clicked.connect(self.toggle_connection)
        
        port_layout.addWidget(port_label)
        port_layout.addWidget(self.port_combo)
        port_layout.addWidget(refresh_btn)
        port_layout.addWidget(self.connect_btn)
        main_layout.addLayout(port_layout)
        
        # Durum göstergesi
        self.status_label = QLabel("🔴 Disconnected")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setFont(QFont("Arial", 14))
        main_layout.addWidget(self.status_label)
        
        # Kontrol tuşları
        controls_label = QLabel("⌨️ CONTROLS")
        controls_label.setAlignment(Qt.AlignCenter)
        controls_label.setFont(QFont("Arial", 16, QFont.Bold))
        main_layout.addWidget(controls_label)
        
        # Tuş grid'i
        grid_layout = QGridLayout()
        
        # Yukarı
        self.btn_w = QPushButton("W\n↑\nUP")
        self.btn_w.setMinimumSize(100, 80)
        self.btn_w.clicked.connect(lambda: self.send_command('W'))
        grid_layout.addWidget(self.btn_w, 0, 1)
        
        # Sol
        self.btn_a = QPushButton("A\n←\nLEFT")
        self.btn_a.setMinimumSize(100, 80)
        self.btn_a.clicked.connect(lambda: self.send_command('A'))
        grid_layout.addWidget(self.btn_a, 1, 0)
        
        # Aşağı
        self.btn_s = QPushButton("S\n↓\nDOWN")
        self.btn_s.setMinimumSize(100, 80)
        self.btn_s.clicked.connect(lambda: self.send_command('S'))
        grid_layout.addWidget(self.btn_s, 1, 1)
        
        # Sağ
        self.btn_d = QPushButton("D\n→\nRIGHT")
        self.btn_d.setMinimumSize(100, 80)
        self.btn_d.clicked.connect(lambda: self.send_command('D'))
        grid_layout.addWidget(self.btn_d, 1, 2)
        
        # Restart
        self.btn_r = QPushButton("R\n🔄\nRESTART")
        self.btn_r.setMinimumSize(100, 80)
        self.btn_r.setStyleSheet("""
            QPushButton {
                background-color: #2d2d2d;
                color: #ff0000;
                border: 2px solid #ff0000;
                border-radius: 5px;
                font-size: 16px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #ff0000;
                color: #000000;
            }
        """)
        self.btn_r.clicked.connect(lambda: self.send_command('R'))
        grid_layout.addWidget(self.btn_r, 0, 2)
        
        grid_layout.setSpacing(10)
        main_layout.addLayout(grid_layout)
        
        # Açıklama
        info_label = QLabel("💡 Click with mouse OR use W/A/S/D/R keys on keyboard")
        info_label.setAlignment(Qt.AlignCenter)
        main_layout.addWidget(info_label)
        
        # Serial monitor
        monitor_label = QLabel("📟 Serial Monitor")
        monitor_label.setFont(QFont("Arial", 12, QFont.Bold))
        main_layout.addWidget(monitor_label)
        
        self.serial_monitor = QTextEdit()
        self.serial_monitor.setReadOnly(True)
        self.serial_monitor.setMaximumHeight(150)
        main_layout.addWidget(self.serial_monitor)
        
        # Temizle butonu
        clear_btn = QPushButton("🗑️ Clear")
        clear_btn.clicked.connect(self.serial_monitor.clear)
        main_layout.addWidget(clear_btn)
        
        # Serial okuma timer'ı
        self.read_timer = QTimer()
        self.read_timer.timeout.connect(self.read_serial)
        
    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_combo.addItem(f"{port.device} - {port.description}")
            
    def toggle_connection(self):
        if self.serial_port and self.serial_port.is_open:
            self.disconnect()
        else:
            self.connect()
            
    def connect(self):
        try:
            port_text = self.port_combo.currentText()
            if not port_text:
                self.serial_monitor.append("❌ Please select a port!")
                return
                
            port_name = port_text.split(" - ")[0]
            
            self.serial_port = serial.Serial(port_name, 115200, timeout=0.1)
            self.status_label.setText("🟢 Connected!")
            self.status_label.setStyleSheet("color: #00ff00;")
            self.connect_btn.setText("🔌 Disconnect")
            self.serial_monitor.append(f"✅ Connected to {port_name}!\n")
            
            # Serial okumayı başlat
            self.read_timer.start(50)  # Her 50ms'de bir oku
            
        except Exception as e:
            self.serial_monitor.append(f"❌ Connection error: {str(e)}\n")
            self.status_label.setText("🔴 Connection Error")
            
    def disconnect(self):
        if self.serial_port:
            self.read_timer.stop()
            self.serial_port.close()
            self.serial_port = None
            self.status_label.setText("🔴 Disconnected")
            self.status_label.setStyleSheet("color: #ff0000;")
            self.connect_btn.setText("🔌 Connect")
            self.serial_monitor.append("❌ Disconnected.\n")
            
    def send_command(self, command):
        if self.serial_port and self.serial_port.is_open:
            try:
                self.serial_port.write(command.encode())
                # Görsel feedback
                self.highlight_button(command)
            except Exception as e:
                self.serial_monitor.append(f"❌ Send error: {str(e)}\n")
        else:
            self.serial_monitor.append("❌ Please connect to ESP32 first!\n")
            
    def highlight_button(self, command):
        buttons = {
            'W': self.btn_w,
            'A': self.btn_a,
            'S': self.btn_s,
            'D': self.btn_d,
            'R': self.btn_r
        }
        
        btn = buttons.get(command)
        if btn:
            original_style = btn.styleSheet()
            btn.setStyleSheet(btn.styleSheet() + "background-color: #00ff00; color: #000000;")
            QTimer.singleShot(100, lambda: btn.setStyleSheet(original_style))
            
    def read_serial(self):
        if self.serial_port and self.serial_port.is_open:
            try:
                if self.serial_port.in_waiting > 0:
                    data = self.serial_port.read(self.serial_port.in_waiting).decode('utf-8', errors='ignore')
                    self.serial_monitor.append(data)
                    # Auto-scroll
                    self.serial_monitor.verticalScrollBar().setValue(
                        self.serial_monitor.verticalScrollBar().maximum()
                    )
            except Exception as e:
                pass  # Hataları sessizce yok say
                
    def keyPressEvent(self, event: QKeyEvent):
        key = event.text().upper()
        if key in ['W', 'A', 'S', 'D', 'R']:
            self.send_command(key)
        elif event.key() == Qt.Key_Up:
            self.send_command('W')
        elif event.key() == Qt.Key_Left:
            self.send_command('A')
        elif event.key() == Qt.Key_Down:
            self.send_command('S')
        elif event.key() == Qt.Key_Right:
            self.send_command('D')
            
    def closeEvent(self, event):
        if self.serial_port:
            self.disconnect()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = SnakeGameController()
    window.show()
    sys.exit(app.exec())