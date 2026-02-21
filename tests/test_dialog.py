# Functional tests for dialog sharding
# Run via `make test-debug` or `make test-release`

import uuid
import pytest


async def _register_user(service_client, suffix=''):
    response = await service_client.post(
        '/user/register',
        json={
            'first_name': f'Test{suffix}',
            'second_name': 'User',
            'birthdate': '1990-01-01',
            'sex': 'male',
            'biography': 'test bio',
            'city': 'Moscow',
            'password': 'secret123',
        },
    )
    assert response.status == 200
    return response.json()['user_id']


async def _login(service_client, user_id):
    response = await service_client.post(
        '/login',
        json={'id': user_id, 'password': 'secret123'},
    )
    assert response.status == 200
    return response.json()['token']


async def test_dialog_send_and_list(service_client):
    """Test basic send and list dialog operations via sharded storage."""
    user1_id = await _register_user(service_client, '1')
    user2_id = await _register_user(service_client, '2')
    token1 = await _login(service_client, user1_id)

    # Send message from user1 to user2
    response = await service_client.post(
        f'/dialog/{user2_id}/send',
        json={'text': 'Hello from user1'},
        headers={'Authorization': f'Bearer {token1}'},
    )
    assert response.status == 200

    # List dialog messages
    response = await service_client.get(
        f'/dialog/{user2_id}/list',
        headers={'Authorization': f'Bearer {token1}'},
    )
    assert response.status == 200
    messages = response.json()
    assert len(messages) == 1
    assert messages[0]['text'] == 'Hello from user1'
    assert messages[0]['from'] == user1_id
    assert messages[0]['to'] == user2_id


async def test_dialog_bidirectional(service_client):
    """Test that messages from both sides appear in the same dialog list."""
    user1_id = await _register_user(service_client, 'A')
    user2_id = await _register_user(service_client, 'B')
    token1 = await _login(service_client, user1_id)
    token2 = await _login(service_client, user2_id)

    # user1 sends to user2
    resp = await service_client.post(
        f'/dialog/{user2_id}/send',
        json={'text': 'Hi'},
        headers={'Authorization': f'Bearer {token1}'},
    )
    assert resp.status == 200

    # user2 replies to user1
    resp = await service_client.post(
        f'/dialog/{user1_id}/send',
        json={'text': 'Hey back'},
        headers={'Authorization': f'Bearer {token2}'},
    )
    assert resp.status == 200

    # user1 lists dialog with user2 — should see both messages
    resp = await service_client.get(
        f'/dialog/{user2_id}/list',
        headers={'Authorization': f'Bearer {token1}'},
    )
    assert resp.status == 200
    messages = resp.json()
    assert len(messages) == 2
    texts = [m['text'] for m in messages]
    assert 'Hi' in texts
    assert 'Hey back' in texts


async def test_dialog_send_missing_text(service_client):
    """Test that missing `text` field returns 400."""
    user1_id = await _register_user(service_client, 'X')
    user2_id = await _register_user(service_client, 'Y')
    token1 = await _login(service_client, user1_id)

    resp = await service_client.post(
        f'/dialog/{user2_id}/send',
        json={'wrong_field': 'oops'},
        headers={'Authorization': f'Bearer {token1}'},
    )
    assert resp.status == 400


async def test_dialog_send_invalid_user_id(service_client):
    """Test that an invalid UUID in the path returns 400."""
    user1_id = await _register_user(service_client, 'Z')
    token1 = await _login(service_client, user1_id)

    resp = await service_client.post(
        '/dialog/not-a-valid-uuid/send',
        json={'text': 'hello'},
        headers={'Authorization': f'Bearer {token1}'},
    )
    assert resp.status == 400


async def test_dialog_shard_routing_consistency(service_client):
    """Verify that repeated sends/reads on the same pair always land on the same shard."""
    user1_id = await _register_user(service_client, 'P')
    user2_id = await _register_user(service_client, 'Q')
    token1 = await _login(service_client, user1_id)

    for i in range(3):
        resp = await service_client.post(
            f'/dialog/{user2_id}/send',
            json={'text': f'message {i}'},
            headers={'Authorization': f'Bearer {token1}'},
        )
        assert resp.status == 200

    resp = await service_client.get(
        f'/dialog/{user2_id}/list',
        headers={'Authorization': f'Bearer {token1}'},
    )
    assert resp.status == 200
    messages = resp.json()
    assert len(messages) == 3
    texts = [m['text'] for m in messages]
    for i in range(3):
        assert f'message {i}' in texts
