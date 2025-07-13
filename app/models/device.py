from . import db
from datetime import datetime

class Device(db.Model):
    __tablename__ = 'devices'
    id = db.Column(db.Integer, primary_key=True)
    device_id = db.Column(db.String(64), unique=True, nullable=False)
    name = db.Column(db.String(128))
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
