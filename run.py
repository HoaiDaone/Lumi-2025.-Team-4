from app import create_app

app = create_app()

# Tạo bảng tự động khi chạy app
with app.app_context():
    from app.utils.db import db
    db.create_all()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
