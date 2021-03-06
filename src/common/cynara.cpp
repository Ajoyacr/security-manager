/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Rafal Krypa <r.krypa@samsung.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 */
/*
 * @file        cynara.cpp
 * @author      Rafal Krypa <r.krypa@samsung.com>
 * @brief       Wrapper class for Cynara interface
 */

#include <cstring>
#include "cynara.h"

#include <dpl/log/log.h>

namespace SecurityManager {

/**
 * Rules for apps and users are organized into set of buckets stored in Cynara.
 * Bucket is set of rules (app, uid, privilege) -> (DENY, ALLOW, BUCKET, ...).
 *  |------------------------|
 *  |      <<allow>>         |
 *  |   PRIVACY_MANAGER      |
 *  |------------------------|
 *  |  A    U   P      policy|
 *  |------------------------|
 *  | app1 uid1 priv1  DENY  |
 *  |  *   uid2 priv2  DENY  |
 *  |  * * *      Bucket:MAIN|
 *  |------------------------|
 *
 * For details about buckets see Cynara documentation.
 *
 * Security Manager currently defines 8 buckets:
 * - PRIVACY_MANAGER - first bucket during search (which is actually default bucket
 *   with empty string as id). If user specifies his preference then required rule
 *   is created here.
 * - MAIN            - holds rules denied by manufacturer, redirects to MANIFESTS
 *   bucket and holds entries for each user pointing to User Type
 *   specific buckets
 * - MANIFESTS       - stores rules needed by installed apps (from package
 *   manifest)
 * - USER_TYPE_ADMIN
 * - USER_TYPE_SYSTEM
 * - USER_TYPE_NORMAL
 * - USER_TYPE_GUEST - they store privileges from templates for apropriate
 *   user type. ALLOW rules only.
 * - ADMIN           - stores custom rules introduced by device administrator.
 *   Ignored if no matching rule found.
 *
 * Below is basic layout of buckets:
 *
 *  |------------------------|
 *  |      <<allow>>         |
 *  |   PRIVACY_MANAGER      |
 *  |                        |
 *  |  * * *      Bucket:MAIN|                         |------------------|
 *  |------------------------|                         |      <<deny>>    |
 *             |                                    |->|     MANIFESTS    |
 *             -----------------                    |  |                  |
 *                             |                    |  |------------------|
 *                             V                    |
 *                     |------------------------|   |
 *                     |       <<deny>>         |---|
 *                     |         MAIN           |
 * |---------------|   |                        |     |-------------------|
 * |    <<deny>>   |<--| * * *  Bucket:MANIFESTS|---->|      <<deny>>     |
 * | USER_TYPE_SYST|   |------------------------|     |  USER_TYPE_NORMAL |
 * |               |        |              |          |                   |
 * |---------------|        |              |          |-------------------|
 *        |                 |              |                    |
 *        |                 V              V                    |
 *        |      |---------------|      |---------------|       |
 *        |      |    <<deny>>   |      |    <<deny>>   |       |
 *        |      |USER_TYPE_GUEST|      |USER_TYPE_ADMIN|       |
 *        |      |               |      |               |       |
 *        |      |---------------|      |---------------|       |
 *        |              |                      |               |
 *        |              |----             -----|               |
 *        |                  |             |                    |
 *        |                  V             V                    |
 *        |                |------------------|                 |
 *        |------------->  |     <<none>>     | <---------------|
 *                         |       ADMIN      |
 *                         |                  |
 *                         |------------------|
 *
 */
CynaraAdmin::BucketsMap CynaraAdmin::Buckets =
{
    { Bucket::PRIVACY_MANAGER, std::string(CYNARA_ADMIN_DEFAULT_BUCKET)},
    { Bucket::MAIN, std::string("MAIN")},
    { Bucket::USER_TYPE_ADMIN, std::string("USER_TYPE_ADMIN")},
    { Bucket::USER_TYPE_NORMAL, std::string("USER_TYPE_NORMAL")},
    { Bucket::USER_TYPE_GUEST, std::string("USER_TYPE_GUEST") },
    { Bucket::USER_TYPE_SYSTEM, std::string("USER_TYPE_SYSTEM")},
    { Bucket::ADMIN, std::string("ADMIN")},
    { Bucket::MANIFESTS, std::string("MANIFESTS")},
};


CynaraAdminPolicy::CynaraAdminPolicy(const std::string &client, const std::string &user,
        const std::string &privilege, int operation,
        const std::string &bucket)
{
    this->client = strdup(client.c_str());
    this->user = strdup(user.c_str());
    this->privilege = strdup(privilege.c_str());
    this->bucket = strdup(bucket.c_str());

    if (this->bucket == nullptr || this->client == nullptr ||
        this->user == nullptr || this->privilege == nullptr) {
        free(this->bucket);
        free(this->client);
        free(this->user);
        free(this->privilege);
        ThrowMsg(CynaraException::OutOfMemory,
                std::string("Error in CynaraAdminPolicy allocation."));
    }

    this->result = operation;
    this->result_extra = nullptr;
}

CynaraAdminPolicy::CynaraAdminPolicy(const std::string &client, const std::string &user,
    const std::string &privilege, const std::string &goToBucket,
    const std::string &bucket)
{
    this->bucket = strdup(bucket.c_str());
    this->client = strdup(client.c_str());
    this->user = strdup(user.c_str());
    this->privilege = strdup(privilege.c_str());
    this->result_extra = strdup(goToBucket.c_str());
    this->result = CYNARA_ADMIN_BUCKET;

    if (this->bucket == nullptr || this->client == nullptr ||
        this->user == nullptr || this->privilege == nullptr ||
        this->result_extra == nullptr) {
        free(this->bucket);
        free(this->client);
        free(this->user);
        free(this->privilege);
        free(this->result_extra);
        ThrowMsg(CynaraException::OutOfMemory,
                std::string("Error in CynaraAdminPolicy allocation."));
    }
}

CynaraAdminPolicy::CynaraAdminPolicy(CynaraAdminPolicy &&that)
{
    bucket = that.bucket;
    client = that.client;
    user = that.user;
    privilege = that.privilege;
    result_extra = that.result_extra;
    result = that.result;

    that.bucket = nullptr;
    that.client = nullptr;
    that.user = nullptr;
    that.privilege = nullptr;
    that.result_extra = nullptr;
}

CynaraAdminPolicy& CynaraAdminPolicy::operator=(CynaraAdminPolicy &&that)
{
    if (this != &that) {
        bucket = that.bucket;
        client = that.client;
        user = that.user;
        privilege = that.privilege;
        result_extra = that.result_extra;
        result = that.result;

        that.bucket = nullptr;
        that.client = nullptr;
        that.user = nullptr;
        that.privilege = nullptr;
        that.result_extra = nullptr;
    };

    return *this;
}

CynaraAdminPolicy::~CynaraAdminPolicy()
{
    free(this->bucket);
    free(this->client);
    free(this->user);
    free(this->privilege);
    free(this->result_extra);
}

static bool checkCynaraError(int result, const std::string &msg)
{
    switch (result) {
        case CYNARA_API_SUCCESS:
        case CYNARA_API_ACCESS_ALLOWED:
            return true;
        case CYNARA_API_ACCESS_DENIED:
            return false;
        case CYNARA_API_MAX_PENDING_REQUESTS:
            ThrowMsg(CynaraException::MaxPendingRequests, msg);
        case CYNARA_API_OUT_OF_MEMORY:
            ThrowMsg(CynaraException::OutOfMemory, msg);
        case CYNARA_API_INVALID_PARAM:
            ThrowMsg(CynaraException::InvalidParam, msg);
        case CYNARA_API_SERVICE_NOT_AVAILABLE:
            ThrowMsg(CynaraException::ServiceNotAvailable, msg);
        case CYNARA_API_METHOD_NOT_SUPPORTED:
            ThrowMsg(CynaraException::MethodNotSupported, msg);
        case CYNARA_API_OPERATION_NOT_ALLOWED:
            ThrowMsg(CynaraException::OperationNotAllowed, msg);
        case CYNARA_API_OPERATION_FAILED:
            ThrowMsg(CynaraException::OperationFailed, msg);
        case CYNARA_API_BUCKET_NOT_FOUND:
            ThrowMsg(CynaraException::BucketNotFound, msg);
        default:
            ThrowMsg(CynaraException::UnknownError, msg);
    }
}

CynaraAdmin::TypeToDescriptionMap CynaraAdmin::TypeToDescription;
CynaraAdmin::DescriptionToTypeMap CynaraAdmin::DescriptionToType;

CynaraAdmin::CynaraAdmin()
    : m_policyDescriptionsInitialized(false)
{
    checkCynaraError(
        cynara_admin_initialize(&m_CynaraAdmin),
        "Cannot connect to Cynara administrative interface.");
}

CynaraAdmin::~CynaraAdmin()
{
    cynara_admin_finish(m_CynaraAdmin);
}

CynaraAdmin &CynaraAdmin::getInstance()
{
    static CynaraAdmin cynaraAdmin;
    return cynaraAdmin;
}

void CynaraAdmin::SetPolicies(const std::vector<CynaraAdminPolicy> &policies)
{
    if (policies.empty()) {
        LogDebug("no policies to set in Cynara.");
        return;
    }

    std::vector<const struct cynara_admin_policy *> pp_policies(policies.size() + 1);

    LogDebug("Sending " << policies.size() << " policies to Cynara");
    for (std::size_t i = 0; i < policies.size(); ++i) {
        pp_policies[i] = static_cast<const struct cynara_admin_policy *>(&policies[i]);
        LogDebug("policies[" << i << "] = {" <<
            ".bucket = " << pp_policies[i]->bucket << ", " <<
            ".client = " << pp_policies[i]->client << ", " <<
            ".user = " << pp_policies[i]->user << ", " <<
            ".privilege = " << pp_policies[i]->privilege << ", " <<
            ".result = " << pp_policies[i]->result << ", " <<
            ".result_extra = " << pp_policies[i]->result_extra << "}");
    }

    pp_policies[policies.size()] = nullptr;

    checkCynaraError(
        cynara_admin_set_policies(m_CynaraAdmin, pp_policies.data()),
        "Error while updating Cynara policy.");
}

void CynaraAdmin::UpdateAppPolicy(
    const std::string &label,
    const std::string &user,
    const std::vector<std::string> &oldPrivileges,
    const std::vector<std::string> &newPrivileges)
{
    std::vector<CynaraAdminPolicy> policies;

    // Perform sort-merge join on oldPrivileges and newPrivileges.
    // Assume that they are already sorted and without duplicates.
    auto oldIter = oldPrivileges.begin();
    auto newIter = newPrivileges.begin();

    while (oldIter != oldPrivileges.end() && newIter != newPrivileges.end()) {
        int compare = oldIter->compare(*newIter);
        if (compare == 0) {
            LogDebug("(user = " << user << " label = " << label << ") " <<
                "keeping privilege " << *newIter);
            ++oldIter;
            ++newIter;
            continue;
        } else if (compare < 0) {
            LogDebug("(user = " << user << " label = " << label << ") " <<
                "removing privilege " << *oldIter);
            policies.push_back(CynaraAdminPolicy(label, user, *oldIter,
                    static_cast<int>(CynaraAdminPolicy::Operation::Delete),
                    Buckets.at(Bucket::MANIFESTS)));
            ++oldIter;
        } else {
            LogDebug("(user = " << user << " label = " << label << ") " <<
                "adding privilege " << *newIter);
            policies.push_back(CynaraAdminPolicy(label, user, *newIter,
                    static_cast<int>(CynaraAdminPolicy::Operation::Allow),
                    Buckets.at(Bucket::MANIFESTS)));
            ++newIter;
        }
    }

    for (; oldIter != oldPrivileges.end(); ++oldIter) {
        LogDebug("(user = " << user << " label = " << label << ") " <<
            "removing privilege " << *oldIter);
        policies.push_back(CynaraAdminPolicy(label, user, *oldIter,
                    static_cast<int>(CynaraAdminPolicy::Operation::Delete),
                    Buckets.at(Bucket::MANIFESTS)));
    }

    for (; newIter != newPrivileges.end(); ++newIter) {
        LogDebug("(user = " << user << " label = " << label << ") " <<
            "adding privilege " << *newIter);
        policies.push_back(CynaraAdminPolicy(label, user, *newIter,
                    static_cast<int>(CynaraAdminPolicy::Operation::Allow),
                    Buckets.at(Bucket::MANIFESTS)));
    }

    SetPolicies(policies);
}

void CynaraAdmin::UserInit(uid_t uid, security_manager_user_type userType)
{
    Bucket bucket;
    std::vector<CynaraAdminPolicy> policies;

    switch (userType) {
        case SM_USER_TYPE_SYSTEM:
            bucket = Bucket::USER_TYPE_SYSTEM;
            break;
        case SM_USER_TYPE_ADMIN:
            bucket = Bucket::USER_TYPE_ADMIN;
            break;
        case SM_USER_TYPE_GUEST:
            bucket = Bucket::USER_TYPE_GUEST;
            break;
        case SM_USER_TYPE_NORMAL:
            bucket = Bucket::USER_TYPE_NORMAL;
            break;
        case SM_USER_TYPE_ANY:
        case SM_USER_TYPE_NONE:
        case SM_USER_TYPE_END:
        default:
            ThrowMsg(CynaraException::InvalidParam, "User type incorrect");
    }

    policies.push_back(CynaraAdminPolicy(CYNARA_ADMIN_WILDCARD,
                                            std::to_string(static_cast<unsigned int>(uid)),
                                            CYNARA_ADMIN_WILDCARD,
                                            Buckets.at(bucket),
                                            Buckets.at(Bucket::MAIN)));

    CynaraAdmin::getInstance().SetPolicies(policies);
}

void CynaraAdmin::ListUsers(std::vector<uid_t> &listOfUsers)
{
    std::vector<CynaraAdminPolicy> tmpListOfUsers;
    CynaraAdmin::getInstance().ListPolicies(
        CynaraAdmin::Buckets.at(Bucket::MAIN),
        CYNARA_ADMIN_WILDCARD,
        CYNARA_ADMIN_ANY,
        CYNARA_ADMIN_WILDCARD,
        tmpListOfUsers);

    for (const auto &tmpUser : tmpListOfUsers) {
        std::string user = tmpUser.user;
        if (!user.compare(CYNARA_ADMIN_WILDCARD))
            continue;
        try {
            listOfUsers.push_back(std::stoul(user));
        } catch (std::invalid_argument &e) {
            LogError("Invalid UID: " << e.what());
            continue;
        };
    };
    LogDebug("Found users: " << listOfUsers.size());
};

void CynaraAdmin::UserRemove(uid_t uid)
{
    std::vector<CynaraAdminPolicy> policies;
    std::string user = std::to_string(static_cast<unsigned int>(uid));

    EmptyBucket(Buckets.at(Bucket::PRIVACY_MANAGER),true,
            CYNARA_ADMIN_ANY, user, CYNARA_ADMIN_ANY);
}

void CynaraAdmin::ListPolicies(
    const std::string &bucketName,
    const std::string &appId,
    const std::string &user,
    const std::string &privilege,
    std::vector<CynaraAdminPolicy> &policies)
{
    struct cynara_admin_policy ** pp_policies = nullptr;

    checkCynaraError(
        cynara_admin_list_policies(m_CynaraAdmin, bucketName.c_str(), appId.c_str(),
            user.c_str(), privilege.c_str(), &pp_policies),
        "Error while getting list of policies for bucket: " + bucketName);

    for (std::size_t i = 0; pp_policies[i] != nullptr; i++) {
        policies.push_back(std::move(*static_cast<CynaraAdminPolicy*>(pp_policies[i])));

        free(pp_policies[i]);
    }

    free(pp_policies);

}

void CynaraAdmin::EmptyBucket(const std::string &bucketName, bool recursive, const std::string &client,
    const std::string &user, const std::string &privilege)
{
    checkCynaraError(
        cynara_admin_erase(m_CynaraAdmin, bucketName.c_str(), static_cast<int>(recursive),
            client.c_str(), user.c_str(), privilege.c_str()),
        "Error while emptying bucket: " + bucketName + ", filter (C, U, P): " +
            client + ", " + user + ", " + privilege);
}

void CynaraAdmin::FetchCynaraPolicyDescriptions(bool forceRefresh)
{
    struct cynara_admin_policy_descr **descriptions = nullptr;

    if (!forceRefresh && m_policyDescriptionsInitialized)
        return;

    // fetch
    checkCynaraError(
        cynara_admin_list_policies_descriptions(m_CynaraAdmin, &descriptions),
        "Error while getting list of policies descriptions from Cynara.");

    if (descriptions[0] == nullptr) {
        LogError("Fetching policies levels descriptions from Cynara returned empty list. "
                "There should be at least 2 entries - Allow and Deny");
        return;
    }

    // reset the state
    m_policyDescriptionsInitialized = false;
    DescriptionToType.clear();
    TypeToDescription.clear();

    // extract strings
    for (int i = 0; descriptions[i] != nullptr; i++) {
        std::string descriptionName(descriptions[i]->name);

        DescriptionToType[descriptionName] = descriptions[i]->result;
        TypeToDescription[descriptions[i]->result] = std::move(descriptionName);

        free(descriptions[i]->name);
        free(descriptions[i]);
    }

    free(descriptions);

    m_policyDescriptionsInitialized = true;
}

void CynaraAdmin::ListPoliciesDescriptions(std::vector<std::string> &policiesDescriptions)
{
    FetchCynaraPolicyDescriptions(false);

    for (const auto &it : TypeToDescription)
        policiesDescriptions.push_back(it.second);
}

std::string CynaraAdmin::convertToPolicyDescription(const int policyType, bool forceRefresh)
{
    FetchCynaraPolicyDescriptions(forceRefresh);

    return TypeToDescription.at(policyType);
}

int CynaraAdmin::convertToPolicyType(const std::string &policy, bool forceRefresh)
{
    FetchCynaraPolicyDescriptions(forceRefresh);

    return DescriptionToType.at(policy);
}
void CynaraAdmin::Check(const std::string &label, const std::string &user, const std::string &privilege,
    const std::string &bucket, int &result, std::string &resultExtra, const bool recursive)
{
    char *resultExtraCstr = nullptr;

    checkCynaraError(
        cynara_admin_check(m_CynaraAdmin, bucket.c_str(), recursive, label.c_str(),
            user.c_str(), privilege.c_str(), &result, &resultExtraCstr),
        "Error while asking cynara admin API for permission for app label: " + label + ", user: "
            + user + " privilege: " + privilege + " bucket: " + bucket);

    if (resultExtraCstr == nullptr)
        resultExtra = "";
    else {
        resultExtra = std::string(resultExtraCstr);
        free(resultExtraCstr);
    }
}

int CynaraAdmin::GetPrivilegeManagerCurrLevel(const std::string &label, const std::string &user,
        const std::string &privilege)
{
    int result;
    std::string resultExtra;

    Check(label, user, privilege, Buckets.at(Bucket::PRIVACY_MANAGER), result, resultExtra, true);

    return result;
}

int CynaraAdmin::GetPrivilegeManagerMaxLevel(const std::string &label, const std::string &user,
        const std::string &privilege)
{
    int result;
    std::string resultExtra;

    Check(label, user, privilege, Buckets.at(Bucket::MAIN), result, resultExtra, true);

    return result;
}

Cynara::Cynara()
{
    int ret;

    ret = eventfd(0, 0);
    if (ret == -1) {
        LogError("Error while creating eventfd: " << strerror(errno));
        ThrowMsg(CynaraException::UnknownError, "Error while creating eventfd");
    }

    // Poll the eventfd for reading
    pollFds[0].fd = ret;
    pollFds[0].events = POLLIN;

    // Temporary, will be replaced by cynara fd when available
    pollFds[1].fd = pollFds[0].fd;
    pollFds[1].events = 0;

    checkCynaraError(
        cynara_async_initialize(&cynara, nullptr, &Cynara::statusCallback, &(pollFds[1])),
        "Cannot connect to Cynara policy interface.");

    thread = std::thread(&Cynara::run, this);
}

Cynara::~Cynara()
{
    LogDebug("Sending terminate event to Cynara thread");
    terminate.store(true);
    threadNotifyPut();
    thread.join();

    // Critical section
    std::lock_guard<std::mutex> guard(mutex);
    cynara_async_finish(cynara);
}

Cynara &Cynara::getInstance()
{
    static Cynara cynara;
    return cynara;
}

void Cynara::threadNotifyPut()
{
    int ret = eventfd_write(pollFds[0].fd, 1);
    if (ret == -1)
        LogError("Unexpected error while writing to eventfd: " << strerror(errno));
}

void Cynara::threadNotifyGet()
{
    eventfd_t value;
    int ret = eventfd_read(pollFds[0].fd, &value);
    if (ret == -1)
        LogError("Unexpected error while reading from eventfd: " << strerror(errno));
}

void Cynara::statusCallback(int oldFd, int newFd, cynara_async_status status,
    void *ptr)
{
    auto cynaraFd = static_cast<struct pollfd *>(ptr);

    LogDebug("Cynara status callback. " <<
        "Status = " << status << ", oldFd = " << oldFd << ", newFd = " << newFd);

    if (newFd == -1) {
        cynaraFd->events = 0;
    } else {
        cynaraFd->fd = newFd;

        switch (status) {
        case CYNARA_STATUS_FOR_READ:
            cynaraFd->events = POLLIN;
            break;

        case CYNARA_STATUS_FOR_RW:
            cynaraFd->events = POLLIN | POLLOUT;
            break;
        }
    }
}

void Cynara::responseCallback(cynara_check_id checkId,
    cynara_async_call_cause cause, int response, void *ptr)
{
    LogDebug("Response for received for Cynara check id: " << checkId);

    auto promise = static_cast<std::promise<bool>*>(ptr);

    switch (cause) {
    case CYNARA_CALL_CAUSE_ANSWER:
        LogDebug("Cynara cause: ANSWER: " << response);
        promise->set_value(response);
        break;

    case CYNARA_CALL_CAUSE_CANCEL:
        LogDebug("Cynara cause: CANCEL");
        promise->set_value(CYNARA_API_ACCESS_DENIED);
        break;

    case CYNARA_CALL_CAUSE_FINISH:
        LogDebug("Cynara cause: FINISH");
        promise->set_value(CYNARA_API_ACCESS_DENIED);
        break;

    case CYNARA_CALL_CAUSE_SERVICE_NOT_AVAILABLE:
        LogError("Cynara cause: SERVICE_NOT_AVAILABLE");

        try {
            ThrowMsg(CynaraException::ServiceNotAvailable,
                "Cynara service not available");
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
        break;
    }
}

void Cynara::run()
{
    LogInfo("Cynara thread started");
    while (true) {
        int ret = poll(pollFds, 2, -1);
        if (ret == -1) {
            if (errno != EINTR)
                LogError("Unexpected error returned by poll: " << strerror(errno));
            continue;
        }

        // Check eventfd for termination signal
        if (pollFds[0].revents) {
            threadNotifyGet();
            if (terminate.load()) {
                LogInfo("Cynara thread terminated");
                return;
            }
        }

        // Check if Cynara fd is ready for processing
        try {
            if (pollFds[1].revents) {
                // Critical section
                std::lock_guard<std::mutex> guard(mutex);

                checkCynaraError(cynara_async_process(cynara),
                    "Unexpected error returned by cynara_async_process");
            }
        } catch (const CynaraException::Base &e) {
            LogError("Error while processing Cynara events: " << e.DumpToString());
        }
    }
}

bool Cynara::check(const std::string &label, const std::string &privilege,
        const std::string &user, const std::string &session)
{
    LogDebug("check: client = " << label << ", user = " << user <<
        ", privilege = " << privilege << ", session = " << session);

    std::promise<bool> promise;
    auto future = promise.get_future();

    // Critical section
    {
        std::lock_guard<std::mutex> guard(mutex);

        int ret = cynara_async_check_cache(cynara,
            label.c_str(), session.c_str(), user.c_str(), privilege.c_str());

        if (ret != CYNARA_API_CACHE_MISS)
            return checkCynaraError(ret, "Error while checking Cynara cache");

        LogDebug("Cynara cache miss");

        cynara_check_id check_id;
        checkCynaraError(
            cynara_async_create_request(cynara,
                label.c_str(), session.c_str(), user.c_str(), privilege.c_str(),
                &check_id, &Cynara::responseCallback, &promise),
            "Cannot check permission with Cynara.");

        threadNotifyPut();
        LogDebug("Waiting for response to Cynara query id " << check_id);
    }

    return future.get();
}

} // namespace SecurityManager
