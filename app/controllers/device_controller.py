from flask import request, jsonify
from ..models.device import Device
from ..utils.db import db

def register_device():
    data = request.get_json()
    new = Device(device_id=data['device_id'], name=data.get('name'))
    db.session.add(new); db.session.commit()
    return jsonify({'message': 'Device registered'}), 201

def list_devices():
    devices = Device.query.all()
    return jsonify([{'device_id': d.device_id, 'name': d.name} for d in devices])
