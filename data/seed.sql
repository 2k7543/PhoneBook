INSERT OR IGNORE INTO users(fullname, email, password)
VALUES ('Demo User', 'demo@phonebook.local', '12345678');

INSERT OR IGNORE INTO contact_groups(user_id, name)
SELECT id, 'Family' FROM users WHERE email = 'demo@phonebook.local';

INSERT OR IGNORE INTO contact_groups(user_id, name)
SELECT id, 'Work' FROM users WHERE email = 'demo@phonebook.local';

INSERT OR IGNORE INTO contacts(user_id, fullname, phone, email, company, address, note)
SELECT id, 'Nguyen Van An', '0901234567', 'an@example.com',
       'Example Company', 'Ha Noi', 'Lien lac thuong xuyen'
FROM users WHERE email = 'demo@phonebook.local';
