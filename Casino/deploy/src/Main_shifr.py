import os

def rotl8(x: int, n: int) -> int:
    return ((x << n) & 0xFF) | (x >> (8 - n))

def rotr8(x: int, n: int) -> int:
    return ((x >> n) | ((x << (8 - n)) & 0xFF)) & 0xFF

def pad_to_blocksize(data: bytes, block_size: int) -> bytes:
    pad_len = block_size - (len(data) % block_size)
    return data + bytes([pad_len] * pad_len)

def unpad(data: bytes) -> bytes:
    pad_len = data[-1]
    if not (1 <= pad_len <= 16):
        raise ValueError("Некорректное дополнение")
    if data[-pad_len:] != bytes([pad_len] * pad_len):
        raise ValueError("Некорректное дополнение")
    return data[:-pad_len]

def simple_block_encrypt_block(block: bytes, key_byte: int) -> bytes:
    out = []
    for b in block:
        x = b ^ key_byte          
        x = rotl8(x, 3)           
        out.append(x)
    return bytes(out)

def simple_block_encrypt(plaintext: bytes, key: int = 0x55, block_size: int = 16) -> bytes:
    padded = pad_to_blocksize(plaintext, block_size)
    key_byte = key & 0xFF  
    ciphertext = b""
    for i in range(0, len(padded), block_size):
        block = padded[i:i+block_size]
        ciphertext += simple_block_encrypt_block(block, key_byte)
    return ciphertext

def block_shifr_main(input_string):
    input_string = input_string.hex()[:32]
    input_string = bytes.fromhex(input_string)
    key_bytes = b"Happy_New_Year_2026"
    key_int = int.from_bytes(key_bytes, byteorder='big')
    encrypted = simple_block_encrypt(input_string, key_int, block_size=16)
    return encrypted


