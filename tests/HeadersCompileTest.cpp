#include "database/Database.h"
#include "database/DatabaseInitializer.h"
#include "models/Contact.h"
#include "models/ContactEmail.h"
#include "models/ContactGroup.h"
#include "models/ContactPhoneNumber.h"
#include "models/User.h"
#include "repositories/AdminLogRepository.h"
#include "repositories/ContactRepository.h"
#include "repositories/GroupRepository.h"
#include "repositories/UserRepository.h"
#include "service/AdminService.h"
#include "service/AuthService.h"
#include "service/ContactService.h"
#include "service/CsvService.h"
#include "service/GroupService.h"
#include "service/PasswordResetService.h"
#include "service/PhoneBookRuntime.h"
#include "service/ServiceException.h"
#include "utils/DateTime.h"
#include "utils/PasswordHasher.h"
#include "utils/Validator.h"

// Includes every public header to ensure the application API compiles together.
int main() {
    return 0;
}
