#!/usr/bin/env python3
"""
Example usage of the atom.secret module.

This script demonstrates the main features of the password management system.
"""

import sys
from datetime import datetime, timedelta

try:
    from atom.secret import (
        Encryption,
        EncryptionMethod,
        EncryptionOptions,
        GenerationOptions,
        PasswordCategory,
        PasswordEntry,
        PasswordGenerator,
        PasswordManager,
        PasswordManagerSettings,
        PasswordValidator,
    )
except ImportError as e:
    print(f"Error importing atom.secret: {e}")
    print("Make sure the module is built and installed correctly.")
    sys.exit(1)


def print_section(title):
    """Print a section header."""
    print("\n" + "=" * 60)
    print(f"  {title}")
    print("=" * 60)


def example_password_generation():
    """Demonstrate password generation."""
    print_section("Password Generation")

    # Generate with default options
    print("\n1. Default password generation:")
    result = PasswordGenerator.generate_password()
    if result.is_success():
        password = result.value()
        print(f"   Generated: {password}")
        print(f"   Length: {len(password)}")

    # Generate with custom options
    print("\n2. Custom password generation:")
    options = GenerationOptions()
    options.length = 20
    options.include_special = True
    options.exclude_ambiguous = True
    options.min_lowercase = 3
    options.min_uppercase = 3
    options.min_digits = 3
    options.min_special = 2

    result = PasswordGenerator.generate_password(options)
    if result.is_success():
        password = result.value()
        print(f"   Generated: {password}")
        print(f"   Length: {len(password)}")

    # Generate memorable password
    print("\n3. Memorable password generation:")
    result = PasswordGenerator.generate_memorable_password(4, "-", True)
    if result.is_success():
        password = result.value()
        print(f"   Generated: {password}")

    # Generate PIN
    print("\n4. PIN generation:")
    result = PasswordGenerator.generate_pin(6)
    if result.is_success():
        pin = result.value()
        print(f"   Generated PIN: {pin}")


def example_password_validation():
    """Demonstrate password validation."""
    print_section("Password Validation")

    test_passwords = [
        ("abc123", "Very weak password"),
        ("password123", "Weak password"),
        ("Password123", "Medium password"),
        ("P@ssw0rd123", "Strong password"),
        ("MyV3ry$ecur3P@ssw0rd!", "Very strong password"),
    ]

    for password, description in test_passwords:
        print(f"\n{description}: '{password}'")
        analysis = PasswordValidator.analyze_password(password)

        print(f"  Strength: {analysis.strength}")
        print(f"  Score: {analysis.score}/100")
        print(f"  Entropy: {analysis.entropy:.2f} bits")
        print("  Characteristics:")
        print(f"    - Lowercase: {analysis.has_lowercase}")
        print(f"    - Uppercase: {analysis.has_uppercase}")
        print(f"    - Digits: {analysis.has_digits}")
        print(f"    - Special: {analysis.has_special}")
        print(f"    - Repeated chars: {analysis.has_repeated_chars}")
        print(f"    - Sequential chars: {analysis.has_sequential_chars}")
        print(f"    - Common password: {analysis.is_common_password}")

        if analysis.suggestions:
            print("  Suggestions:")
            for suggestion in analysis.suggestions:
                print(f"    - {suggestion}")

        # Estimate crack time
        crack_time = PasswordValidator.estimate_crack_time(password)
        if crack_time < 60:
            time_str = f"{crack_time:.2f} seconds"
        elif crack_time < 3600:
            time_str = f"{crack_time/60:.2f} minutes"
        elif crack_time < 86400:
            time_str = f"{crack_time/3600:.2f} hours"
        elif crack_time < 31536000:
            time_str = f"{crack_time/86400:.2f} days"
        else:
            time_str = f"{crack_time/31536000:.2f} years"
        print(f"  Estimated crack time: {time_str}")


def example_encryption():
    """Demonstrate encryption and decryption."""
    print_section("Encryption and Decryption")

    plaintext = "This is a secret message!"
    password = "MySecurePassword123!"

    print(f"\nOriginal message: '{plaintext}'")
    print(f"Password: '{password}'")

    # Encrypt with default options (AES-GCM)
    print("\n1. Encrypting with AES-GCM...")
    options = EncryptionOptions()
    options.encryption_method = EncryptionMethod.AES_GCM

    encrypt_result = Encryption.encrypt(plaintext, password, options)
    if encrypt_result.is_success():
        encrypted_data = encrypt_result.value()
        print("   Encrypted successfully!")
        print(f"   Ciphertext length: {len(encrypted_data.ciphertext)} bytes")
        print(f"   IV length: {len(encrypted_data.iv)} bytes")
        print(f"   Salt length: {len(encrypted_data.salt)} bytes")
        print(f"   Tag length: {len(encrypted_data.tag)} bytes")
        print(f"   Method: {encrypted_data.method}")
        print(f"   Key iterations: {encrypted_data.key_iterations}")

        # Decrypt
        print("\n2. Decrypting...")
        decrypt_result = Encryption.decrypt(encrypted_data, password)
        if decrypt_result.is_success():
            decrypted = decrypt_result.value()
            print("   Decrypted successfully!")
            print(f"   Decrypted message: '{decrypted}'")
            print(f"   Match: {decrypted == plaintext}")
        else:
            print(f"   Decryption failed: {decrypt_result.error()}")

        # Try with wrong password
        print("\n3. Trying with wrong password...")
        wrong_decrypt_result = Encryption.decrypt(encrypted_data, "WrongPassword")
        if wrong_decrypt_result.is_error():
            print(f"   Correctly failed: {wrong_decrypt_result.error()}")
    else:
        print(f"   Encryption failed: {encrypt_result.error()}")


def example_password_manager():
    """Demonstrate password manager usage."""
    print_section("Password Manager")

    # Create and initialize manager
    print("\n1. Initializing password manager...")
    manager = PasswordManager()
    settings = PasswordManagerSettings()
    settings.auto_lock_timeout_seconds = 300
    settings.min_password_length = 12
    settings.password_expiry_days = 90

    if not manager.initialize("MyMasterPassword123!", settings):
        print("   Failed to initialize manager!")
        return
    print("   Manager initialized successfully!")

    # Store some passwords
    print("\n2. Storing passwords...")

    entries = [
        (
            "github.com",
            "github_user@example.com",
            "https://github.com",
            PasswordCategory.Work,
        ),
        (
            "gmail.com",
            "personal@gmail.com",
            "https://gmail.com",
            PasswordCategory.Personal,
        ),
        ("bank.com", "customer123", "https://bank.com", PasswordCategory.Finance),
    ]

    for key, username, url, category in entries:
        entry = PasswordEntry()

        # Generate a password
        password_result = PasswordGenerator.generate_password()
        if password_result.is_success():
            entry.password = password_result.value()
        else:
            entry.password = "DefaultPassword123!"

        entry.username = username
        entry.url = url
        entry.category = category
        entry.tags = ["example", "demo"]
        entry.created = datetime.now()
        entry.modified = datetime.now()
        entry.expires = datetime.now() + timedelta(days=90)

        if manager.store_password(key, entry):
            print(f"   Stored: {key}")
        else:
            print(f"   Failed to store: {key}")

    # Retrieve passwords
    print("\n3. Retrieving passwords...")
    for key, _, _, _ in entries:
        retrieved = manager.retrieve_password(key)
        if not retrieved.is_empty():
            print(f"   {key}:")
            print(f"     Username: {retrieved.username}")
            print(f"     Password: {retrieved.password}")
            print(f"     Category: {retrieved.category}")

    # Search passwords
    print("\n4. Searching passwords...")
    results = manager.search_passwords("gmail")
    print(f"   Found {len(results)} results for 'gmail':")
    for key, entry in results:
        print(f"     {key}: {entry.username}")

    # Filter by category
    print("\n5. Filtering by category (Finance)...")
    finance_results = manager.filter_by_category(PasswordCategory.Finance)
    print(f"   Found {len(finance_results)} finance passwords:")
    for key, entry in finance_results:
        print(f"     {key}: {entry.username}")

    # Get all keys
    print("\n6. Getting all keys...")
    all_keys = manager.get_all_keys()
    print(f"   Total passwords stored: {len(all_keys)}")
    for key in all_keys:
        print(f"     - {key}")

    # Get statistics
    print("\n7. Getting statistics...")
    stats = manager.get_statistics()
    print(f"   Total entries: {stats.total_entries}")
    print(f"   Expired entries: {stats.expired_entries}")
    print(f"   Weak passwords: {stats.weak_passwords}")
    print(f"   Duplicate passwords: {stats.duplicate_passwords}")

    # Export to JSON
    print("\n8. Exporting to JSON...")
    export_result = manager.export_to_json()
    if export_result.is_success():
        json_data = export_result.value()
        print(f"   Exported successfully! ({len(json_data)} bytes)")
    else:
        print(f"   Export failed: {export_result.error()}")

    # Lock manager
    print("\n9. Locking manager...")
    manager.lock()
    print(f"   Manager locked: {manager.is_locked()}")

    # Try to access while locked
    print("\n10. Trying to access while locked...")
    locked_entry = manager.retrieve_password("github.com")
    print(f"   Entry is empty: {locked_entry.is_empty()}")

    # Unlock
    print("\n11. Unlocking manager...")
    if manager.unlock("MyMasterPassword123!"):
        print("   Manager unlocked successfully!")
    else:
        print("   Failed to unlock manager!")


def main():
    """Run all examples."""
    print("\n" + "=" * 60)
    print("  Atom Secret Module - Python Bindings Examples")
    print("=" * 60)

    try:
        example_password_generation()
        example_password_validation()
        example_encryption()
        example_password_manager()

        print("\n" + "=" * 60)
        print("  All examples completed successfully!")
        print("=" * 60 + "\n")

    except Exception as e:
        print(f"\nError running examples: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
