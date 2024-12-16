#! /usr/bin/python3

# colcon build --packages-select pyqt_rcv_cam_ihm_ros2
# . install/setup.bash
# ros2 run pyqt_rcv_cam_ihm_ros2 rcv_cam_ihm

import rclpy
from rclpy.node import Node

from std_msgs.msg import String
from sensor_msgs.msg import Image

import sys
from PyQt5 import QtCore
from PyQt5.QtCore import QTimer
from PyQt5.QtGui import QColor, QPixmap, QImage
from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QGraphicsScene, QGraphicsView, QGridLayout
 
########################################################################
class MainWindow(QMainWindow):
	
    def __init__(self, parent=None):
        super(MainWindow, self).__init__(parent)

        # Elements de l'IHM
        self.setMinimumSize(800, 800)    
        self.setWindowTitle("ROS2 RECEIVE CAM IHM") 
         
         # Disposition des Elements
        widget = QWidget()
        self.setCentralWidget(widget)

        self.graphicsScene = QGraphicsScene()
        self.graphicsView = QGraphicsView()
        self.graphicsScene.setBackgroundBrush(QColor(0, 0, 0))
        self.graphicsView.setScene(self.graphicsScene)
        self.qpixmap = QPixmap()
        m_layout = QGridLayout(widget);
        m_layout.addWidget(self.graphicsView,0,0)

        # ROS2
        rclpy.init(args=None) 
        self.node = Node('PC_HOST_Node')
 
        self.i = 0
        
        self.subscription = self.node.create_subscription(
            Image,
            '/camera/src_frame',
            self.img_callback,
            10)
           
        self.subscription  # prevent unused variable warning
        
        self.timer = QTimer()
        self.timer.timeout.connect(self.onTimerTick)   
        self.timer.start(5)      
 
    def img_callback(self, img):
        image = QImage(img.data, img.width, img.height, QImage.Format_RGB888 )
        self.graphicsScene.clear();
        self.graphicsScene.addPixmap(self.qpixmap.fromImage(image)) 
        
    def onTimerTick(self):  
        rclpy.spin_once(self.node)
               
########################################################################
def main(args=None):
 
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()

    sys.exit(app.exec())

if __name__ == '__main__':
    main()
########################################################################
   
