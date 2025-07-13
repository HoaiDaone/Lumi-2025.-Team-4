from flask import Blueprint
from ..controllers.device_controller import register_device, list_devices

device_bp = Blueprint('device_bp', __name__)
device_bp.route('/register', methods=['POST'])(register_device)
device_bp.route('/', methods=['GET'])(list_devices)
