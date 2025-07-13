
import pytest
from app import create_app
import uuid

@pytest.fixture
def client():
    app = create_app()
    app.config['TESTING'] = True
    with app.test_client() as client:
        with app.app_context():
            yield client

def test_register_device(client):
    global test_device_id
    test_device_id = f"test_{uuid.uuid4().hex[:8]}"
    response = client.post('/devices/register', json={"device_id": test_device_id, "name": "Test Device"})
    assert response.status_code in (200, 201)

def test_list_devices(client):
    response = client.get('/devices/')
    assert response.status_code == 200

def test_send_message(client):
    sender_id = globals().get('test_device_id', f"test_{uuid.uuid4().hex[:8]}")
    receiver_id = f"test_{uuid.uuid4().hex[8:16]}"
    response = client.post('/messages/send', json={
        "sender_id": sender_id,
        "receiver_id": receiver_id,
        "content": "Hello"
    })
    assert response.status_code in (200, 201)

def test_get_messages(client):
    device_id = globals().get('test_device_id', f"test_{uuid.uuid4().hex[:8]}")
    response = client.get(f'/messages/inbox?device_id={device_id}')
    assert response.status_code == 200
