// User hierarchy (design Fig 2A). User is abstract: roleName() and permissions()
// are pure virtual and each subclass overrides them (polymorphism, FR-2).
#pragma once
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "domain/Models.hpp"

namespace lms {

class User {
public:
    User(int id, std::string username, std::string fullName, bool active, std::string createdAt)
        : userId_(id), username_(std::move(username)), fullName_(std::move(fullName)), active_(active),
          createdAt_(std::move(createdAt)) {}
    virtual ~User() = default;

    int getUserId() const { return userId_; }
    const std::string& getUsername() const { return username_; }
    const std::string& getFullName() const { return fullName_; }
    bool isActive() const { return active_; }
    const std::string& getCreatedAt() const { return createdAt_; }

    virtual std::string roleName() const = 0;
    virtual Role role() const = 0;
    virtual std::set<Permission> permissions() const = 0;
    virtual std::string referenceNo() const { return ""; }              // student / employee number
    virtual std::optional<int> departmentId() const { return std::nullopt; }
    bool can(Permission p) const { return permissions().count(p) > 0; }

private:
    int userId_;
    std::string username_;
    std::string fullName_;
    bool active_;
    std::string createdAt_;
};

class Student : public User {
public:
    Student(int id, std::string un, std::string fn, bool act, std::string created, std::string studentNo)
        : User(id, std::move(un), std::move(fn), act, std::move(created)), studentNo_(std::move(studentNo)) {}
    std::string roleName() const override { return "Student"; }
    Role role() const override { return Role::Student; }
    std::set<Permission> permissions() const override {
        return {Permission::SUBMIT_WORK, Permission::VIEW_COURSES, Permission::VIEW_OWN_MARKS};
    }
    std::string referenceNo() const override { return studentNo_; }
private:
    std::string studentNo_;
};

class Lecturer : public User {
public:
    Lecturer(int id, std::string un, std::string fn, bool act, std::string created, std::string employeeNo, int dept)
        : User(id, std::move(un), std::move(fn), act, std::move(created)), employeeNo_(std::move(employeeNo)),
          departmentId_(dept) {}
    std::string roleName() const override { return "Lecturer"; }
    Role role() const override { return Role::Lecturer; }
    std::set<Permission> permissions() const override {
        return {Permission::CREATE_ASSIGNMENT, Permission::GRANT_EXTENSION, Permission::MARK_SUBMISSION,
                Permission::VIEW_COURSES};
    }
    std::string referenceNo() const override { return employeeNo_; }
    std::optional<int> departmentId() const override { return departmentId_; }
private:
    std::string employeeNo_;
    int departmentId_;
};

class HeadOfDepartment : public User {
public:
    HeadOfDepartment(int id, std::string un, std::string fn, bool act, std::string created, std::string employeeNo,
                     int dept)
        : User(id, std::move(un), std::move(fn), act, std::move(created)), employeeNo_(std::move(employeeNo)),
          departmentId_(dept) {}
    std::string roleName() const override { return "HoD"; }
    Role role() const override { return Role::HoD; }
    std::set<Permission> permissions() const override {
        return {Permission::MODERATE_MARKS, Permission::RELEASE_MARKS, Permission::VIEW_COURSES};
    }
    std::string referenceNo() const override { return employeeNo_; }
    std::optional<int> departmentId() const override { return departmentId_; }
private:
    std::string employeeNo_;
    int departmentId_;
};

class Administrator : public User {
public:
    Administrator(int id, std::string un, std::string fn, bool act, std::string created)
        : User(id, std::move(un), std::move(fn), act, std::move(created)) {}
    std::string roleName() const override { return "Admin"; }
    Role role() const override { return Role::Admin; }
    std::set<Permission> permissions() const override {
        return {Permission::MANAGE_USERS, Permission::MANAGE_COURSES, Permission::ENROL_STUDENTS,
                Permission::VIEW_COURSES};
    }
};

}  // namespace lms
