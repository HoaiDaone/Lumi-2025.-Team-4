from flask import Blueprint
from ..controllers.message_controller import send_message, get_messages

message_bp = Blueprint('message_bp', __name__)
message_bp.route('/send', methods=['POST'])(send_message)
message_bp.route('/inbox', methods=['GET'])(get_messages)
