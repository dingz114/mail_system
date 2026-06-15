from pathlib import Path
import sys
import time

import pymysql
from pymysql.constants import CLIENT


ROOT_PASSWORD = "865486"


def connect_root():
    last_error = None
    for password in (ROOT_PASSWORD, ""):
        for host in ("127.0.0.1", "localhost"):
            try:
                return pymysql.connect(
                    host=host,
                    port=3306,
                    user="root",
                    password=password,
                    autocommit=True,
                    client_flag=CLIENT.MULTI_STATEMENTS,
                    connect_timeout=5,
                )
            except Exception as exc:
                last_error = exc
    raise RuntimeError(f"Cannot connect to local MySQL as root: {last_error}")


def wait_for_mysql():
    last_error = None
    for _ in range(30):
        try:
            return connect_root()
        except Exception as exc:
            last_error = exc
            time.sleep(0.5)
    raise RuntimeError(f"MySQL is not ready: {last_error}")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: init_database.py <schema.sql>", file=sys.stderr)
        return 2

    schema_path = Path(sys.argv[1])
    schema = schema_path.read_text(encoding="utf-8")

    conn = wait_for_mysql()
    try:
        with conn.cursor() as cur:
            cur.execute(
                "ALTER USER 'root'@'localhost' "
                f"IDENTIFIED WITH mysql_native_password BY '{ROOT_PASSWORD}'"
            )
            cur.execute(
                "CREATE USER IF NOT EXISTS 'root'@'127.0.0.1' "
                f"IDENTIFIED WITH mysql_native_password BY '{ROOT_PASSWORD}'"
            )
            cur.execute(
                "ALTER USER 'root'@'127.0.0.1' "
                f"IDENTIFIED WITH mysql_native_password BY '{ROOT_PASSWORD}'"
            )
            cur.execute(
                "GRANT ALL PRIVILEGES ON *.* TO 'root'@'localhost' "
                "WITH GRANT OPTION"
            )
            cur.execute(
                "GRANT ALL PRIVILEGES ON *.* TO 'root'@'127.0.0.1' "
                "WITH GRANT OPTION"
            )
            cur.execute("FLUSH PRIVILEGES")
            cur.execute(schema)
            while cur.nextset():
                pass
            cur.execute("SHOW TABLES FROM mail_system")
            tables = ", ".join(row[0] for row in cur.fetchall())
            print(f"MySQL ready. Tables: {tables}")
    finally:
        conn.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
