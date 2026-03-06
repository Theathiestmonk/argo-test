import sys
if sys.prefix == '/usr':
    sys.real_prefix = sys.prefix
    sys.prefix = sys.exec_prefix = '/home/argo/Downloads/robot_wss/install/simulation_pkg'
