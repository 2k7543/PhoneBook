#include "service/PasswordResetService.h"
#include "utils/DateTime.h"
#include "utils/Validator.h"
using namespace std;

// Issues and redeems expiring, single-use password-reset tokens.
PasswordResetService::PasswordResetService(Database& db, UserRepository& users, PasswordHasher& hasher)
    : db_(db), users_(users), hasher_(hasher) {}

// Creates a random token for an active account and stores only its digest.
optional<string> PasswordResetService::requestReset(const string& email) {
    auto user = users_.findByEmail(Validator::toLower(Validator::trim(email)));
    if (!user || !user->isActive) return std::nullopt;

    string token = PasswordHasher::randomString(40);
    const string tokenHash = PasswordHasher::hashToken(token);
    if (token.empty() || tokenHash.empty())
        throw ServiceException(ErrorCode::InvalidInput,
                               "Không thể tạo mã đặt lại an toàn");
    users_.saveResetToken(user->idUser, tokenHash,
                          DateTime::afterMinutes(kTokenMinutes));
    return token;
}

// Validates the token and password, then updates both records atomically.
void PasswordResetService::resetPassword(const string& token, const string& newPassword) {
    if (!Validator::isValidPassword(newPassword))
        throw ServiceException(ErrorCode::InvalidInput, "Mật khẩu mới tối thiểu 8 ký tự, gồm cả chữ và số");

    const string tokenHash = PasswordHasher::hashToken(token);
    auto info = users_.findResetToken(tokenHash);
    if (!info || info->used)
        throw ServiceException(ErrorCode::InvalidInput, "Mã đặt lại mật khẩu không hợp lệ");
    if (DateTime::isExpired(info->expiresAt))
        throw ServiceException(ErrorCode::Expired, "Mã đặt lại mật khẩu đã hết hạn");

    Transaction tx(db_);
    users_.updatePassword(info->userId, hasher_.hash(newPassword));
    users_.markResetTokenUsed(tokenHash);
    tx.commit();
}
