#pragma once
enum class Responses : uint8_t {
    kSuccessConnection,
    kOk,
    kNoAccess,
    kError,
    kBadLogin,
    kMessage,
    kEmptyResponse,
    kSuccessSignIn,
    kLoginIsAlreadyUsed,
    kSuccessSignUp,
    kBadLoginFormat,
    kFileData,
    kFileSize,
    kDisconnect
};
enum class Queries : uint8_t { kEcho, kSignUp, kSignIn, kLogOut, kLoginFormat, kSendFile };
enum class FileStatus : uint8_t { kDone, kBusy, kDisconnect, kError, kNotFound, kConnectionError };
