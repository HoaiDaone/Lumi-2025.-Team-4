
from app import create_app
from sqlalchemy import create_engine
import os

app = create_app()

# Tự động tạo database nếu chưa có
def create_database_if_not_exists():
    from app.config import Config
    db_url = Config.SQLALCHEMY_DATABASE_URI
    db_name = os.getenv('DB_NAME')
    # Tạo kết nối đến MySQL không có db_name
    url_no_db = db_url.rsplit('/', 1)[0]
    engine = create_engine(url_no_db)
    with engine.connect() as conn:
        conn.execute(f"CREATE DATABASE IF NOT EXISTS {db_name}")

create_database_if_not_exists()

# Tạo bảng tự động khi chạy app
with app.app_context():
    from app.utils.db import db
    db.create_all()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
