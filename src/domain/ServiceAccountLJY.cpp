#include "domain/ServiceAccountLJY.h"

#include <utility>

ServiceAccountLJY::ServiceAccountLJY(int accountId, int personId,
                                     ServiceTypeLJY serviceType, std::string externalId,
                                     std::string nickname, std::string appliedOn)
    : accountId_(accountId), personId_(personId), serviceType_(serviceType),
      externalId_(std::move(externalId)), nickname_(std::move(nickname)),
      appliedOn_(std::move(appliedOn))
{
}

int ServiceAccountLJY::accountId() const noexcept { return accountId_; }
int ServiceAccountLJY::personId() const noexcept { return personId_; }
ServiceTypeLJY ServiceAccountLJY::serviceType() const noexcept { return serviceType_; }
const std::string& ServiceAccountLJY::externalId() const noexcept { return externalId_; }
const std::string& ServiceAccountLJY::nickname() const noexcept { return nickname_; }
const std::string& ServiceAccountLJY::appliedOn() const noexcept { return appliedOn_; }
