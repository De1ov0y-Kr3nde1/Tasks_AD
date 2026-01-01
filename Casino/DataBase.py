import sqlite3
import os

DB_PATH = os.path.join(os.path.dirname(__file__), 'casino.db')

def get_db():
    return sqlite3.connect(DB_PATH)

def init_db():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL,
            plain_password TEXT NOT NULL,
            balance INTEGER NOT NULL DEFAULT 1000,
            has_boost BOOLEAN NOT NULL DEFAULT 0,
            total_bets INTEGER NOT NULL DEFAULT 0,
            wins INTEGER NOT NULL DEFAULT 0,
            losses INTEGER NOT NULL DEFAULT 0,
            boost_count INTEGER NOT NULL DEFAULT 0,
            double_count INTEGER NOT NULL DEFAULT 0,
            extra_life_count INTEGER NOT NULL DEFAULT 0,
            thousand_bonus_count INTEGER NOT NULL DEFAULT 0
        )
    ''')
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS secrets (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            salt BLOB NOT NULL
        )
    ''')
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS flag (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            content TEXT NOT NULL
        )
    ''')
    
    FIXED_SALT = b'Santa_Brings_You_CTF_Flags_2025!'
    cursor.execute('SELECT salt FROM secrets WHERE id = 1')
    if cursor.fetchone() is None:
        cursor.execute('INSERT INTO secrets (id, salt) VALUES (1, ?)', (FIXED_SALT,))
    
    FLAG_CONTENT = "Flag{1t_i5_a_R3a1_F1a9}"
    cursor.execute('SELECT content FROM flag WHERE id = 1')
    if cursor.fetchone() is None:
        cursor.execute('INSERT INTO flag (id, content) VALUES (1, ?)', (FLAG_CONTENT,))
    
    try:
        cursor.execute(
            "INSERT INTO users (username, password, plain_password, balance, has_boost) VALUES (?, ?, ?, ?, ?)",
            ("Christmas_Father", "dB2TlVyx8LbLmOZSOmb6PSkwEWwbsFrBvCoUIsfIKzI", "dB2TlVyx8LbLmOZSOmb6PSkwEWwbsFrBvCoUIsfIKzI", 1000000, 1)
        )
    except sqlite3.IntegrityError:
        pass
    
    conn.commit()
    conn.close()

def get_secret_salt():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute('SELECT salt FROM secrets WHERE id = 1')
    row = cursor.fetchone()
    conn.close()
    if row:
        return row[0]
    else:
        raise RuntimeError("Соль не найдена в БД. Вызовите init_db() сначала.")

def get_flag():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute('SELECT content FROM flag WHERE id = 1')
    row = cursor.fetchone()
    conn.close()
    if row:
        return row[0]
    else:
        raise RuntimeError("Флаг не найден в БД. Вызовите init_db() сначала.")