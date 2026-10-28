"""Reservation retry for the qc paste-a-checklist example.

Return the original reservation only when the call is a retry and the
age is at most 24 hours.
"""


def same_reservation(is_retry, age_hours):
    return is_retry and age_hours <= 24


def main():
    cases = [
        (True, 10, True),
        (True, 25, False),
        (False, 10, False),
        (False, 25, False),
    ]
    for is_retry, age_hours, expected in cases:
        got = same_reservation(is_retry, age_hours)
        print(
            f"retry={is_retry} age={age_hours} "
            f"same={got} expected={expected}"
        )


if __name__ == "__main__":
    main()
