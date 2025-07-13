from flask import Flask
from .config import Config
from app.utils.db import db, migrate

def create_app():
    app = Flask(__name__)
    app.config.from_object(Config)

    # Khởi tạo DB + Migrations
    db.init_app(app)
    migrate.init_app(app, db)

    # Đăng ký blueprint routes
    from .routes.device_routes import device_bp
    from .routes.message_routes import message_bp
    app.register_blueprint(device_bp, url_prefix='/devices')
    app.register_blueprint(message_bp, url_prefix='/messages')

    return app
