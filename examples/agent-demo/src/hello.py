"""Tiny demo app for qc agent workflow."""

def greet(name: str) -> str:
    if not name:
        return "hello, stranger"
    return f"hello, {name}"


def main() -> None:
    print(greet("world"))


if __name__ == "__main__":
    main()
