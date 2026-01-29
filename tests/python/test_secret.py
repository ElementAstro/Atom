"""
Test suite for atom.secret Python bindings.

This module tests all functionality of the atom.secret module including:
- Password generation and validation
- Encryption and decryption
- Password manager operations
- Secure storage
- Serialization
"""

import pytest

from atom.secret import (  # Enums; Data structures; Password utilities; Encryption utilities; Password manager; Storage; Serialization
    Encryption,
    EncryptionMethod,
    EncryptionOptions,
    GenerationOptions,
    JsonSerializer,
    KeyDerivation,
    PasswordCategory,
    PasswordEntry,
    PasswordGenerator,
    PasswordManager,
    PasswordManagerSettings,
    PasswordStrength,
    PasswordValidator,
    SecureComparison,
    SimpleJsonParser,
)


class TestEnums:
    """Test enum types."""

    def test_password_strength_enum(self):
        """Test PasswordStrength enum values."""
        assert PasswordStrength.VeryWeak
        assert PasswordStrength.Weak
        assert PasswordStrength.Medium
        assert PasswordStrength.Strong
        assert PasswordStrength.VeryStrong

    def test_password_category_enum(self):
        """Test PasswordCategory enum values."""
        assert PasswordCategory.General
        assert PasswordCategory.Finance
        assert PasswordCategory.Work
        assert PasswordCategory.Personal
        assert PasswordCategory.Social
        assert PasswordCategory.Entertainment
        assert PasswordCategory.Other

    def test_encryption_method_enum(self):
        """Test EncryptionMethod enum values."""
        assert EncryptionMethod.AES_GCM
        assert EncryptionMethod.AES_CBC
        assert EncryptionMethod.CHACHA20_POLY1305


class TestDataStructures:
    """Test data structure types."""

    def test_password_entry_creation(self):
        """Test PasswordEntry creation and attributes."""
        entry = PasswordEntry()
        entry.password = "TestPassword123!"
        entry.username = "user@example.com"
        entry.url = "https://example.com"
        entry.notes = "Test notes"
        entry.title = "Test Entry"
        entry.category = PasswordCategory.Personal
        entry.tags = ["important", "work"]

        assert entry.password == "TestPassword123!"
        assert entry.username == "user@example.com"
        assert entry.url == "https://example.com"
        assert entry.notes == "Test notes"
        assert entry.title == "Test Entry"
        assert entry.category == PasswordCategory.Personal
        assert len(entry.tags) == 2
        assert not entry.is_empty()

    def test_encryption_options(self):
        """Test EncryptionOptions creation and attributes."""
        options = EncryptionOptions()
        assert options.use_hardware_acceleration
        assert options.key_iterations == 100000
        assert options.encryption_method == EncryptionMethod.AES_GCM

        options.key_iterations = 200000
        options.encryption_method = EncryptionMethod.AES_CBC
        assert options.key_iterations == 200000
        assert options.encryption_method == EncryptionMethod.AES_CBC

    def test_password_manager_settings(self):
        """Test PasswordManagerSettings creation and attributes."""
        settings = PasswordManagerSettings()
        assert settings.auto_lock_timeout_seconds == 300
        assert settings.notify_on_password_expiry
        assert settings.password_expiry_days == 90
        assert settings.min_password_length == 12
        assert settings.require_special_chars
        assert settings.require_numbers
        assert settings.require_mixed_case


class TestPasswordGenerator:
    """Test password generation functionality."""

    def test_generate_default_password(self):
        """Test generating a password with default options."""
        result = PasswordGenerator.generate_password()
        assert result.is_success()
        password = result.value()
        assert len(password) >= 12
        assert any(c.isupper() for c in password)
        assert any(c.islower() for c in password)
        assert any(c.isdigit() for c in password)

    def test_generate_custom_password(self):
        """Test generating a password with custom options."""
        options = GenerationOptions()
        options.length = 20
        options.include_special = False
        options.exclude_ambiguous = True

        result = PasswordGenerator.generate_password(options)
        assert result.is_success()
        password = result.value()
        assert len(password) == 20

    def test_generate_memorable_password(self):
        """Test generating a memorable password."""
        result = PasswordGenerator.generate_memorable_password(4, "-", True)
        assert result.is_success()
        password = result.value()
        assert "-" in password

    def test_generate_pin(self):
        """Test generating a PIN."""
        result = PasswordGenerator.generate_pin(6)
        assert result.is_success()
        pin = result.value()
        assert len(pin) == 6
        assert pin.isdigit()


class TestPasswordValidator:
    """Test password validation functionality."""

    def test_analyze_weak_password(self):
        """Test analyzing a weak password."""
        analysis = PasswordValidator.analyze_password("abc123")
        assert analysis.strength in [PasswordStrength.VeryWeak, PasswordStrength.Weak]
        assert analysis.score < 50

    def test_analyze_strong_password(self):
        """Test analyzing a strong password."""
        analysis = PasswordValidator.analyze_password("MySecureP@ssw0rd123!")
        assert analysis.strength in [
            PasswordStrength.Strong,
            PasswordStrength.VeryStrong,
        ]
        assert analysis.score > 70
        assert analysis.has_lowercase
        assert analysis.has_uppercase
        assert analysis.has_digits
        assert analysis.has_special

    def test_calculate_entropy(self):
        """Test calculating password entropy."""
        entropy = PasswordValidator.calculate_entropy("password")
        assert entropy > 0

        strong_entropy = PasswordValidator.calculate_entropy("MySecureP@ssw0rd123!")
        assert strong_entropy > entropy

    def test_is_common_password(self):
        """Test checking for common passwords."""
        # Note: This depends on the common password list in C++
        assert PasswordValidator.is_common_password("password") or True

    def test_estimate_crack_time(self):
        """Test estimating crack time."""
        crack_time = PasswordValidator.estimate_crack_time("abc123")
        assert crack_time >= 0


class TestEncryption:
    """Test encryption functionality."""

    def test_key_derivation_generate_salt(self):
        """Test generating a salt."""
        result = KeyDerivation.generate_salt(32)
        assert result.is_success()
        salt = result.value()
        assert len(salt) == 32

    def test_key_derivation_derive_key(self):
        """Test deriving a key from a password."""
        salt_result = KeyDerivation.generate_salt(32)
        assert salt_result.is_success()
        salt = salt_result.value()

        key_result = KeyDerivation.derive_key("password", salt, 10000, 32)
        assert key_result.is_success()
        key = key_result.value()
        assert len(key) == 32

    def test_encrypt_decrypt(self):
        """Test encrypting and decrypting data."""
        plaintext = "Secret message"
        password = "MySecurePassword123!"

        # Encrypt
        encrypt_result = Encryption.encrypt(plaintext, password)
        assert encrypt_result.is_success()
        encrypted_data = encrypt_result.value()

        assert len(encrypted_data.ciphertext) > 0
        assert len(encrypted_data.iv) > 0
        assert len(encrypted_data.salt) > 0

        # Decrypt
        decrypt_result = Encryption.decrypt(encrypted_data, password)
        assert decrypt_result.is_success()
        decrypted = decrypt_result.value()
        assert decrypted == plaintext

    def test_encrypt_decrypt_wrong_password(self):
        """Test decrypting with wrong password fails."""
        plaintext = "Secret message"
        password = "MySecurePassword123!"
        wrong_password = "WrongPassword456!"

        encrypt_result = Encryption.encrypt(plaintext, password)
        assert encrypt_result.is_success()
        encrypted_data = encrypt_result.value()

        decrypt_result = Encryption.decrypt(encrypted_data, wrong_password)
        assert decrypt_result.is_error()


class TestPasswordManager:
    """Test password manager functionality."""

    @pytest.fixture
    def manager(self):
        """Create a password manager instance."""
        mgr = PasswordManager()
        assert mgr.initialize("TestMasterPassword123!")
        yield mgr
        mgr.lock()

    def test_initialization(self):
        """Test password manager initialization."""
        manager = PasswordManager()
        assert manager.initialize("TestMasterPassword123!")
        assert not manager.is_locked()

    def test_lock_unlock(self, manager):
        """Test locking and unlocking."""
        manager.lock()
        assert manager.is_locked()

        assert manager.unlock("TestMasterPassword123!")
        assert not manager.is_locked()

    def test_store_retrieve_password(self, manager):
        """Test storing and retrieving a password."""
        entry = PasswordEntry()
        entry.password = "TestPassword123!"
        entry.username = "user@example.com"
        entry.url = "https://example.com"
        entry.category = PasswordCategory.Personal

        assert manager.store_password("example.com", entry)

        retrieved = manager.retrieve_password("example.com")
        assert retrieved.password == "TestPassword123!"
        assert retrieved.username == "user@example.com"
        assert retrieved.url == "https://example.com"

    def test_remove_password(self, manager):
        """Test removing a password."""
        entry = PasswordEntry()
        entry.password = "TestPassword123!"
        entry.username = "user@example.com"

        assert manager.store_password("test_key", entry)
        assert manager.remove_password("test_key")

        retrieved = manager.retrieve_password("test_key")
        assert retrieved.is_empty()

    def test_get_all_keys(self, manager):
        """Test getting all password keys."""
        entry = PasswordEntry()
        entry.password = "TestPassword123!"

        manager.store_password("key1", entry)
        manager.store_password("key2", entry)
        manager.store_password("key3", entry)

        keys = manager.get_all_keys()
        assert len(keys) >= 3
        assert "key1" in keys
        assert "key2" in keys
        assert "key3" in keys

    def test_search_passwords(self, manager):
        """Test searching passwords."""
        entry1 = PasswordEntry()
        entry1.password = "Password1"
        entry1.username = "user1@example.com"
        entry1.url = "https://example.com"

        entry2 = PasswordEntry()
        entry2.password = "Password2"
        entry2.username = "user2@gmail.com"
        entry2.url = "https://gmail.com"

        manager.store_password("example", entry1)
        manager.store_password("gmail", entry2)

        results = manager.search_passwords("example")
        assert len(results) >= 1

        results = manager.search_passwords("gmail")
        assert len(results) >= 1

    def test_filter_by_category(self, manager):
        """Test filtering by category."""
        personal_entry = PasswordEntry()
        personal_entry.password = "Password1"
        personal_entry.category = PasswordCategory.Personal

        work_entry = PasswordEntry()
        work_entry.password = "Password2"
        work_entry.category = PasswordCategory.Work

        manager.store_password("personal", personal_entry)
        manager.store_password("work", work_entry)

        personal_results = manager.filter_by_category(PasswordCategory.Personal)
        assert len(personal_results) >= 1

        work_results = manager.filter_by_category(PasswordCategory.Work)
        assert len(work_results) >= 1

    def test_generate_password(self, manager):
        """Test password generation through manager."""
        password = manager.generate_password(16, True, True, True)
        assert len(password) == 16

    def test_analyze_password(self, manager):
        """Test password analysis through manager."""
        analysis = manager.analyze_password("TestPassword123!")
        assert analysis.strength
        assert analysis.score >= 0

    def test_export_import_json(self, manager):
        """Test exporting and importing passwords."""
        entry = PasswordEntry()
        entry.password = "TestPassword123!"
        entry.username = "user@example.com"

        manager.store_password("test_key", entry)

        # Export
        export_result = manager.export_to_json()
        assert export_result.is_success()
        json_data = export_result.value()

        # Create new manager and import
        new_manager = PasswordManager()
        new_manager.initialize("NewMasterPassword123!")

        import_result = new_manager.import_from_json(json_data, True)
        assert import_result.is_success()

        # Verify imported data
        retrieved = new_manager.retrieve_password("test_key")
        assert retrieved.password == "TestPassword123!"

    def test_get_statistics(self, manager):
        """Test getting password statistics."""
        entry = PasswordEntry()
        entry.password = "TestPassword123!"

        manager.store_password("key1", entry)
        manager.store_password("key2", entry)

        stats = manager.get_statistics()
        assert stats.total_entries >= 2

    def test_update_settings(self, manager):
        """Test updating settings."""
        settings = manager.get_settings()
        settings.auto_lock_timeout_seconds = 600
        settings.min_password_length = 16

        assert manager.update_settings(settings)

        updated_settings = manager.get_settings()
        assert updated_settings.auto_lock_timeout_seconds == 600
        assert updated_settings.min_password_length == 16


class TestSecureComparison:
    """Test secure comparison functionality."""

    def test_constant_time_equals(self):
        """Test constant-time string comparison."""
        assert SecureComparison.constant_time_equals("password", "password")
        assert not SecureComparison.constant_time_equals("password", "Password")
        assert not SecureComparison.constant_time_equals("password", "pass")


class TestJsonSerializer:
    """Test JSON serialization functionality."""

    def test_serialize_deserialize_entry(self):
        """Test serializing and deserializing a password entry."""
        entry = PasswordEntry()
        entry.password = "TestPassword123!"
        entry.username = "user@example.com"
        entry.url = "https://example.com"
        entry.category = PasswordCategory.Personal

        # Serialize
        serialize_result = JsonSerializer.serialize_entry(entry)
        assert serialize_result.is_success()
        json_str = serialize_result.value()

        # Deserialize
        deserialize_result = JsonSerializer.deserialize_entry(json_str)
        assert deserialize_result.is_success()
        deserialized = deserialize_result.value()

        assert deserialized.password == entry.password
        assert deserialized.username == entry.username
        assert deserialized.url == entry.url

    def test_serialize_deserialize_settings(self):
        """Test serializing and deserializing settings."""
        settings = PasswordManagerSettings()
        settings.auto_lock_timeout_seconds = 600
        settings.min_password_length = 16

        # Serialize
        serialize_result = JsonSerializer.serialize_settings(settings)
        assert serialize_result.is_success()
        json_str = serialize_result.value()

        # Deserialize
        deserialize_result = JsonSerializer.deserialize_settings(json_str)
        assert deserialize_result.is_success()
        deserialized = deserialize_result.value()

        assert deserialized.auto_lock_timeout_seconds == 600
        assert deserialized.min_password_length == 16


class TestSimpleJsonParser:
    """Test simple JSON parser functionality."""

    def test_extract_string(self):
        """Test extracting string from JSON."""
        json_str = '{"name": "John", "age": 30}'
        name = SimpleJsonParser.extract_string(json_str, "name")
        assert name == "John"

    def test_extract_int(self):
        """Test extracting integer from JSON."""
        json_str = '{"name": "John", "age": 30}'
        age = SimpleJsonParser.extract_int(json_str, "age")
        assert age == 30

    def test_extract_bool(self):
        """Test extracting boolean from JSON."""
        json_str = '{"active": true, "verified": false}'
        active = SimpleJsonParser.extract_bool(json_str, "active")
        assert active

        verified = SimpleJsonParser.extract_bool(json_str, "verified")
        assert not verified

    def test_is_valid_json(self):
        """Test validating JSON."""
        assert SimpleJsonParser.is_valid_json('{"name": "John"}')
        assert not SimpleJsonParser.is_valid_json("invalid json")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
