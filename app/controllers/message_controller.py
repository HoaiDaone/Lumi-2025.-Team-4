from flask import request, jsonify
from ..models.message import Message
from ..utils.db import db

def send_message():
    data = request.get_json()
    msg = Message(
        sender_id=data['sender_id'],
        receiver_id=data['receiver_id'],
        content=data['content']
    )
    db.session.add(msg); db.session.commit()
    return jsonify({'message': 'Sent'}), 201

def get_messages():
    device_id = request.args.get('device_id')
    msgs = Message.query.filter_by(receiver_id=device_id).order_by(Message.timestamp).all()
    return jsonify([{
        'from': m.sender_id, 'content': m.content, 'time': m.timestamp.isoformat()
    } for m in msgs])
