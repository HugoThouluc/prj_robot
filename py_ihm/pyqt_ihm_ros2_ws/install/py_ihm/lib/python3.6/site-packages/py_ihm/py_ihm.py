import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from sensor_msgs.msg import Image
import sys
from PyQt5 import QtCore
from PyQt5.QtCore import QTimer, Qt
from PyQt5.QtGui import QImage, QPixmap
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QGraphicsScene, QGraphicsView, QVBoxLayout, QHBoxLayout, QPushButton, QLabel, QSlider, QGridLayout, QSplitter

########################################################################
class MainWindow(QMainWindow):
    
    def __init__(self, parent=None):
        super(MainWindow, self).__init__(parent)

        # Elements de l'IHM
        self.setMinimumSize(800, 800)
        self.setWindowTitle("ROS2 IHM")

        # Création des éléments pour afficher l'image de la caméra
        self.graphicsScene = QGraphicsScene()
        self.graphicsView = QGraphicsView(self.graphicsScene)
        self.graphicsView.setFixedSize(1000, 800)
        self.qpixmap = QPixmap()

        # Boutons de modes
        self.btn_sendmsg = QPushButton('SEND ROS MESSAGE', self)
        self.btn_sendmsg.clicked.connect(self.onPushSend)

        self.btn_mode_manuel = QPushButton('MODE MANUEL', self)
        self.btn_mode_manuel.clicked.connect(self.onPushManuel)

        self.btn_mode_aleatoire = QPushButton('MODE ALEATOIRE', self)
        self.btn_mode_aleatoire.clicked.connect(self.onPushAleatoire)

        self.btn_mode_tracking = QPushButton('MODE TRACKING', self)
        self.btn_mode_tracking.clicked.connect(self.onPushTracking)

        # Boutons directionnels Z, Q, S, D
        self.btn_z = QPushButton('Z', self)
        self.btn_z.clicked.connect(lambda: self.publish_direction('1'))

        self.btn_q = QPushButton('Q', self)
        self.btn_q.clicked.connect(lambda: self.publish_direction('3'))

        self.btn_s = QPushButton('S', self)
        self.btn_s.clicked.connect(lambda: self.publish_direction('2'))

        self.btn_d = QPushButton('D', self)
        self.btn_d.clicked.connect(lambda: self.publish_direction('4'))

        self.btn_stop = QPushButton('STOP', self)
        self.btn_stop.clicked.connect(lambda: self.publish_direction('0'))

        widget = QWidget()
        self.setCentralWidget(widget)

        main_layout = QHBoxLayout(widget)
        splitter = QSplitter(Qt.Horizontal)

        image_widget = QWidget()
        image_layout = QVBoxLayout(image_widget)
        image_layout.addWidget(self.graphicsView)
        
        buttons_widget = QWidget()
        buttons_layout = QVBoxLayout(buttons_widget)

        # Layout pour les boutons de mode
        vb_modes = QVBoxLayout()
        vb_modes.addWidget(self.btn_sendmsg)
        vb_modes.addWidget(self.btn_mode_manuel)
        vb_modes.addWidget(self.btn_mode_aleatoire)
        vb_modes.addWidget(self.btn_mode_tracking)
        vb_modes.addStretch()
        
        # Layout pour les boutons directionnels
        grid_controls = QGridLayout()
        grid_controls.addWidget(self.btn_z, 0, 1)
        grid_controls.addWidget(self.btn_q, 1, 0)
        grid_controls.addWidget(self.btn_s, 1, 1)
        grid_controls.addWidget(self.btn_d, 1, 2)
        grid_controls.addWidget(self.btn_stop, 0, 2)

        buttons_layout.addLayout(vb_modes)
        buttons_layout.addLayout(grid_controls)
        
        splitter.addWidget(image_widget)
        splitter.addWidget(buttons_widget)

        main_layout.addWidget(splitter)

        # ROS2
        rclpy.init(args=None) 
        self.node = Node('py_ihm_node')
        self.publisher_ = self.node.create_publisher(String, 'pyqt_topic_send', 10)
        
        # Subscription pour l'image de la caméra
        self.subscription = self.node.create_subscription(
            Image,
            '/camera/src_frame',
            self.img_callback,
            10)

        self.timer = QTimer()
        self.timer.timeout.connect(self.onTimerTick)   
        self.timer.start(5) 

    def img_callback(self, img):
        # Convertir l'image reçue en QImage
        image = QImage(img.data, img.width, img.height, QImage.Format_RGB888)
        
        # Convertir QImage en QPixmap et afficher dans la scène
        self.qpixmap = QPixmap.fromImage(image)
        self.graphicsScene.clear()  # Clear the scene before adding the new image
        self.graphicsScene.addPixmap(self.qpixmap)

    def onPushSend(self):
        msg = String()
        msg.data = 'Hello World'
        self.publisher_.publish(msg)
        print('Publishing: "%s"' % msg.data)

    def onPushManuel(self):
        msg = String()
        msg.data = 'm'
        self.publisher_.publish(msg)
        print('Publishing: "%s"' % msg.data)

    def onPushAleatoire(self):
        msg = String()
        msg.data = 'a'
        self.publisher_.publish(msg)
        print('Publishing: "%s"' % msg.data)

    def onPushTracking(self):
        msg = String()
        msg.data = 't'
        self.publisher_.publish(msg)
        print('Publishing: "%s"' % msg.data)

    def publish_direction(self, direction):
        msg = String()
        msg.data = direction
        self.publisher_.publish(msg)
        print(f'Publishing: "{msg.data}"')

    def onTimerTick(self):  
        rclpy.spin_once(self.node, executor=None, timeout_sec=0.01)

########################################################################
def main(args=None):
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()

    sys.exit(app.exec())

if __name__ == '__main__':
    main()

