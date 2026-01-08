import base64
from argon2 import low_level
import DataBase

def generate_password_from_nickname(nickname_or_bytes: bytes) -> str:
    secret_data = nickname_or_bytes  
    secret_solt = DataBase.get_secret_salt()  

    raw_hash = low_level.hash_secret_raw(
        secret=secret_data,              
        salt=secret_solt,                
        time_cost=7,
        memory_cost=65536,
        parallelism=4,
        hash_len=32,
        type=low_level.Type.ID
    )
    return base64.urlsafe_b64encode(raw_hash).decode().strip("=")