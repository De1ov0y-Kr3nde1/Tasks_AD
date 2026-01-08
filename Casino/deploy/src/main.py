import os
import random
from flask import Flask, request, render_template, redirect, url_for, session
import Argon2id
import Hand_shifr
import Main_shifr
import DataBase
import sqlite3

DataBase.init_db()

def generate_password_from_username(username: str) -> str:
    if not username or not isinstance(username, str):
        raise ValueError("Invalid username")
    encrypted = Hand_shifr.aes_ecb_encrypt(username.encode(), Hand_shifr.key_secret)
    transformed = Main_shifr.block_shifr_main(encrypted)
    if isinstance(transformed, str):
        transformed = transformed.encode()
    password = Argon2id.generate_password_from_nickname(transformed)
    return password

app = Flask(__name__)
app.secret_key = os.environ.get('SECRET_KEY', os.urandom(24))

@app.after_request
def security_headers(response):
    response.headers['X-Content-Type-Options'] = 'nosniff'
    response.headers['X-Frame-Options'] = 'DENY'
    return response

def get_user(user_id):
    if not isinstance(user_id, int) and not (isinstance(user_id, str) and user_id.isdigit()):
        return None
    conn = None
    try:
        conn = DataBase.get_db()
        cursor = conn.cursor()
        cursor.execute('''
            SELECT 
                id, 
                username, 
                password, 
                plain_password, 
                balance, 
                has_boost,
                total_bets, 
                wins, 
                losses,
                boost_count, 
                double_count, 
                extra_life_count, 
                thousand_bonus_count
            FROM users 
            WHERE id = ?
        ''', (int(user_id),))
        return cursor.fetchone()
    finally:
        if conn:
            conn.close()

def html_escape(text: str) -> str:
    if not isinstance(text, str):
        return ""
    return (text
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
    )

@app.route('/')
def index():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))

    boost_count = user[9] if user[9] is not None else 0
    double_count = user[10] if user[10] is not None else 0
    extra_life_count = user[11] if user[11] is not None else 0
    thousand_bonus_count = user[12] if user[12] is not None else 0

    return render_template(
        'view.html',
        username=html_escape(user[1]),
        balance=user[4],
        boost_count=boost_count,
        double_count=double_count,
        extra_life_count=extra_life_count,
        thousand_bonus_count=thousand_bonus_count,
        creator="Christmas_Father",
        last_play=session.get('last_play'),
        error=session.pop('error', None)
    )

@app.route('/shop')
def shop():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))

    boost_count = user[9] if user[9] is not None else 0
    double_count = user[10] if user[10] is not None else 0
    extra_life_count = user[11] if user[11] is not None else 0
    thousand_bonus_count = user[12] if user[12] is not None else 0

    return render_template('shop.html',
        balance=user[4],
        boost_count=boost_count,
        double_count=double_count,
        extra_life_count=extra_life_count,
        thousand_bonus_count=thousand_bonus_count
    )

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username', '').strip()
        password = request.form.get('password', '')
        if not username or not password:
            return render_template('login.html', error="Заполните все поля")
        conn = None
        try:
            conn = DataBase.get_db()
            cursor = conn.cursor()
            cursor.execute('SELECT id, username, password FROM users WHERE username = ?', (username,))
            user = cursor.fetchone()
            if user and user[2] == password:
                session['user_id'] = user[0]
                return redirect(url_for('index'))
            return render_template('login.html', error="Неверный ник или пароль")
        finally:
            if conn:
                conn.close()
    return render_template('login.html')

@app.route('/register', methods=['GET', 'POST'])
def register():
    if request.method == 'POST':
        username = request.form.get('username', '').strip()
        if not username:
            return render_template('register.html', error="Введите ник")
        if username == "Christmas_Father":
            return render_template('register.html', error="Этот ник зарезервирован")
        if len(username) > 32:
            return render_template('register.html', error="Ник слишком длинный")
        conn = None
        try:
            conn = DataBase.get_db()
            cursor = conn.cursor()
            cursor.execute('SELECT 1 FROM users WHERE username = ?', (username,))
            if cursor.fetchone():
                return render_template('register.html', error="Ник уже занят")
            password = generate_password_from_username(username)
            cursor.execute('''
                INSERT INTO users (
                    username, password, plain_password,
                    balance, has_boost,
                    boost_count, double_count, extra_life_count, thousand_bonus_count
                ) VALUES (?, ?, ?, 1000, 0, 0, 0, 0, 0)
            ''', (username, password, password))
            conn.commit()
            session['user_id'] = cursor.lastrowid
            return redirect(url_for('profile'))
        except sqlite3.IntegrityError:
            return render_template('register.html', error="Ошибка регистрации")
        finally:
            if conn:
                conn.close()
    return render_template('register.html')

@app.route('/profile')
def profile():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))
    return render_template('profile.html',
        username=html_escape(user[1]),
        plain_password=user[3],
        balance=user[4],
        total_bets=user[6],
        wins=user[7],
        losses=user[8],
        is_admin=(user[1] == "Christmas_Father")
    )

@app.route('/play', methods=['POST'])
def play():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))
    
    use_boost = request.form.get('use_boost') == '1'
    use_double = request.form.get('use_double') == '1'
    use_extra_life = request.form.get('use_extra_life') == '1'
    use_thousand_bonus = request.form.get('use_thousand_bonus') == '1'

    try:
        from_val = int(request.form.get('from', 1))
        to_val = int(request.form.get('to', 50))
        bet = int(request.form.get('bet', 10))
    except (ValueError, TypeError):
        session['error'] = "Некорректные данные"
        return redirect(url_for('index'))

    a = max(1, min(100, min(from_val, to_val)))
    b = max(1, min(100, max(from_val, to_val)))
    count = b - a + 1
    if count < 1 or count > 49:
        session['error'] = "Диапазон должен содержать 1–49 чисел"
        return redirect(url_for('index'))
    
    if bet < 1 or bet > 1000000:
        session['error'] = "Ставка должна быть от 1 до 1 000 000"
        return redirect(url_for('index'))

    if user[4] < bet:
        session['error'] = "Недостаточно средств"
        return redirect(url_for('index'))

    boost_count = user[9] if user[9] is not None else 0
    double_count = user[10] if user[10] is not None else 0
    extra_life_count = user[11] if user[11] is not None else 0
    thousand_bonus_count = user[12] if user[12] is not None else 0

    if use_boost and boost_count <= 0:
        session['error'] = "У вас нет бонуса 'Увеличение шанса'"
        return redirect(url_for('index'))
    if use_double and double_count <= 0:
        session['error'] = "У вас нет бонуса 'Удвоение выигрыша'"
        return redirect(url_for('index'))
    if use_extra_life and extra_life_count <= 0:
        session['error'] = "У вас нет бонуса 'Дополнительная жизнь'"
        return redirect(url_for('index'))
    if use_thousand_bonus and thousand_bonus_count <= 0:
        session['error'] = "У вас нет бонуса 'Премия 1000'"
        return redirect(url_for('index'))

    conn = None
    try:
        conn = DataBase.get_db()
        cursor = conn.cursor()
        new_balance = user[4] - bet
        cursor.execute('UPDATE users SET balance = ?, total_bets = total_bets + 1 WHERE id = ?', (new_balance, session['user_id']))

        if use_boost:
            cursor.execute('UPDATE users SET boost_count = boost_count - 1 WHERE id = ?', (session['user_id'],))
        if use_double:
            cursor.execute('UPDATE users SET double_count = double_count - 1 WHERE id = ?', (session['user_id'],))
        if use_thousand_bonus:
            cursor.execute('UPDATE users SET thousand_bonus_count = thousand_bonus_count - 1 WHERE id = ?', (session['user_id'],))

        local_rng = random.Random()
        local_rng.seed(int(session['user_id']))
        number = local_rng.randint(1, 100)
        win = 0
        if a <= number <= b:
            coef = 49 / count
            if use_boost:
                coef = 100 / count
            win = int(bet * coef)
            if use_double:
                win *= 2
            new_balance += win + bet
            if use_thousand_bonus:
                new_balance += 1000  
            cursor.execute('UPDATE users SET balance = ?, wins = wins + 1 WHERE id = ?', (new_balance, session['user_id']))
        else:
            if use_extra_life:
                cursor.execute('UPDATE users SET extra_life_count = extra_life_count - 1 WHERE id = ?', (session['user_id'],))
                new_balance += 100
                cursor.execute('UPDATE users SET balance = ? WHERE id = ?', (new_balance, session['user_id'],))
            cursor.execute('UPDATE users SET losses = losses + 1 WHERE id = ?', (session['user_id'],))

        conn.commit()
        session['last_play'] = {'number': number, 'win': win, 'bet': bet}
        return redirect(url_for('index'))
    finally:
        if conn:
            conn.close()
@app.route('/buy_flag')
def buy_flag():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))
    
    if user[4] >= 100000:  
        conn = None
        try:
            conn = DataBase.get_db()
            cursor = conn.cursor()
            cursor.execute('UPDATE users SET balance = balance - 100000 WHERE id = ?', (session['user_id'],))
            conn.commit()
            flag = DataBase.get_flag()
            return render_template('flag.html', flag=flag)
        finally:
            if conn:
                conn.close()
    session['error'] = "Недостаточно средств"
    return redirect(url_for('index'))

@app.route('/buy_boost', methods=['POST'])
def buy_boost():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))
    if user[4] >= 5000:
        conn = None
        try:
            conn = DataBase.get_db()
            cursor = conn.cursor()
            cursor.execute('UPDATE users SET balance = balance - 5000, boost_count = boost_count + 1 WHERE id = ?', (session['user_id'],))
            conn.commit()
        finally:
            if conn:
                conn.close()
    return redirect(url_for('shop'))

@app.route('/buy_double', methods=['POST'])
def buy_double():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))
    if user[4] >= 500:
        conn = None
        try:
            conn = DataBase.get_db()
            cursor = conn.cursor()
            cursor.execute('UPDATE users SET balance = balance - 500, double_count = double_count + 1 WHERE id = ?', (session['user_id'],))
            conn.commit()
        finally:
            if conn:
                conn.close()
    return redirect(url_for('shop'))

@app.route('/buy_extra_life', methods=['POST'])
def buy_extra_life():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))
    if user[4] >= 500:
        conn = None
        try:
            conn = DataBase.get_db()
            cursor = conn.cursor()
            cursor.execute('UPDATE users SET balance = balance - 500, extra_life_count = extra_life_count + 1 WHERE id = ?', (session['user_id'],))
            conn.commit()
        finally:
            if conn:
                conn.close()
    return redirect(url_for('shop'))

@app.route('/buy_thousand_bonus', methods=['POST'])
def buy_thousand_bonus():
    if 'user_id' not in session:
        return redirect(url_for('login'))
    user = get_user(session['user_id'])
    if not user:
        session.clear()
        return redirect(url_for('login'))
    if user[4] >= 500:
        conn = None
        try:
            conn = DataBase.get_db()
            cursor = conn.cursor()
            cursor.execute('UPDATE users SET balance = balance - 500, thousand_bonus_count = thousand_bonus_count + 1 WHERE id = ?', (session['user_id'],))
            conn.commit()
        finally:
            if conn:
                conn.close()
    return redirect(url_for('shop'))

@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('login'))

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8000)