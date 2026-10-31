#pragma once
namespace lms { inline const char* const kSchemaSql = R"LMSSQL(PRAGMA foreign_keys = ON;
 
CREATE TABLE IF NOT EXISTS Departments (
    department_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT NOT NULL UNIQUE,
    created_at    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);
 
CREATE TABLE IF NOT EXISTS Users (
    user_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,            -- Argon2id string; salt is embedded
    full_name     TEXT NOT NULL,
    role          TEXT NOT NULL CHECK (role IN ('Admin','Lecturer','HoD','Student')),
    reference_no  TEXT,
    department_id INTEGER,
    is_active     INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    created_at    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    FOREIGN KEY (department_id) REFERENCES Departments(department_id) ON DELETE RESTRICT,
    CHECK ((role IN ('Lecturer','HoD') AND department_id IS NOT NULL)
           OR role IN ('Admin','Student'))
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_one_hod_per_department
    ON Users(department_id) WHERE role = 'HoD';
 
CREATE TABLE IF NOT EXISTS Courses (
    course_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    course_code   TEXT NOT NULL UNIQUE,
    title         TEXT NOT NULL,
    department_id INTEGER NOT NULL,
    lecturer_id   INTEGER,
    created_at    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    FOREIGN KEY (department_id) REFERENCES Departments(department_id) ON DELETE RESTRICT,
    FOREIGN KEY (lecturer_id)   REFERENCES Users(user_id)             ON DELETE SET NULL
);
 
CREATE TABLE IF NOT EXISTS Enrolments (
    enrolment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    student_id   INTEGER NOT NULL,
    course_id    INTEGER NOT NULL,
    enrolled_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    UNIQUE (student_id, course_id),
    FOREIGN KEY (student_id) REFERENCES Users(user_id)     ON DELETE RESTRICT,
    FOREIGN KEY (course_id)  REFERENCES Courses(course_id) ON DELETE CASCADE
);
 
CREATE TABLE IF NOT EXISTS Assignments (
    assignment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id     INTEGER NOT NULL,
    title         TEXT NOT NULL,
    deadline      TEXT NOT NULL,            -- UTC ISO-8601
    max_mark      REAL NOT NULL CHECK (max_mark > 0),
    created_by    INTEGER NOT NULL,
    created_at    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    FOREIGN KEY (course_id)  REFERENCES Courses(course_id) ON DELETE RESTRICT,
    FOREIGN KEY (created_by) REFERENCES Users(user_id)     ON DELETE RESTRICT
);
 
CREATE TABLE IF NOT EXISTS Extensions (
    extension_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    assignment_id     INTEGER NOT NULL,
    student_id        INTEGER NOT NULL,
    extended_deadline TEXT NOT NULL,        -- UTC ISO-8601
    reason            TEXT NOT NULL,
    granted_by        INTEGER NOT NULL,
    created_at        TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    FOREIGN KEY (assignment_id) REFERENCES Assignments(assignment_id) ON DELETE RESTRICT,
    FOREIGN KEY (student_id)    REFERENCES Users(user_id)             ON DELETE RESTRICT,
    FOREIGN KEY (granted_by)    REFERENCES Users(user_id)             ON DELETE RESTRICT
);
CREATE INDEX IF NOT EXISTS ix_ext_lookup ON Extensions(assignment_id, student_id);
 
CREATE TABLE IF NOT EXISTS Submissions (
    submission_id INTEGER PRIMARY KEY AUTOINCREMENT,
    assignment_id INTEGER NOT NULL,
    student_id    INTEGER NOT NULL,
    file_path     TEXT NOT NULL,
    submitted_at  TEXT NOT NULL,            -- UTC ISO-8601, latest submission
    version       INTEGER NOT NULL DEFAULT 1 CHECK (version >= 1),
    days_late     INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0),
    status        TEXT NOT NULL DEFAULT 'Submitted'
                  CHECK (status IN ('Submitted','Late','Marked','Moderated','Released')),
    created_at    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    updated_at    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    UNIQUE (assignment_id, student_id),
    FOREIGN KEY (assignment_id) REFERENCES Assignments(assignment_id) ON DELETE RESTRICT,
    FOREIGN KEY (student_id)    REFERENCES Users(user_id)             ON DELETE RESTRICT
);
 
CREATE TABLE IF NOT EXISTS Marks (
    mark_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id  INTEGER NOT NULL UNIQUE,
    raw_mark       REAL NOT NULL CHECK (raw_mark >= 0),
    penalty_points REAL NOT NULL DEFAULT 0 CHECK (penalty_points >= 0),
    final_mark     REAL NOT NULL CHECK (final_mark >= 0),
    feedback       TEXT NOT NULL,
    marked_by      INTEGER NOT NULL,
    marked_at      TEXT NOT NULL,
    review_due_at  TEXT NOT NULL,           -- marked_at + 24 h (auto-release)
    moderated_by   INTEGER,
    moderated_at   TEXT,
    released_at    TEXT,
    FOREIGN KEY (submission_id) REFERENCES Submissions(submission_id) ON DELETE RESTRICT,
    FOREIGN KEY (marked_by)     REFERENCES Users(user_id)             ON DELETE RESTRICT,
    FOREIGN KEY (moderated_by)  REFERENCES Users(user_id)             ON DELETE RESTRICT
);
 
CREATE TABLE IF NOT EXISTS AuditLog (
    log_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    mark_id     INTEGER NOT NULL,
    user_id     INTEGER NOT NULL,
    action_type TEXT NOT NULL
                CHECK (action_type IN ('MARK_CREATED','MARK_CHANGED','MARK_MODERATED',
                       'MARK_REJECTED','MARK_RELEASED','MARK_AUTO_RELEASED')),
    old_value   TEXT,
    new_value   TEXT,
    reason      TEXT NOT NULL,
    changed_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    FOREIGN KEY (mark_id) REFERENCES Marks(mark_id) ON DELETE RESTRICT,
    FOREIGN KEY (user_id) REFERENCES Users(user_id) ON DELETE RESTRICT   -- keeps audit history
);
 
-- Audit log and extensions are append-only
CREATE TRIGGER IF NOT EXISTS trg_audit_no_update BEFORE UPDATE ON AuditLog
BEGIN SELECT RAISE(ABORT, 'AuditLog is append-only'); END;
CREATE TRIGGER IF NOT EXISTS trg_audit_no_delete BEFORE DELETE ON AuditLog
BEGIN SELECT RAISE(ABORT, 'AuditLog is append-only'); END;
CREATE TRIGGER IF NOT EXISTS trg_ext_no_update BEFORE UPDATE ON Extensions
BEGIN SELECT RAISE(ABORT, 'Extensions are append-only'); END;
CREATE TRIGGER IF NOT EXISTS trg_ext_no_delete BEFORE DELETE ON Extensions
BEGIN SELECT RAISE(ABORT, 'Extensions are append-only'); END;
 
-- Indexes (NFR-1: 800 students)
CREATE INDEX IF NOT EXISTS ix_courses_dept   ON Courses(department_id);
CREATE INDEX IF NOT EXISTS ix_enrol_course   ON Enrolments(course_id);
CREATE INDEX IF NOT EXISTS ix_assign_course  ON Assignments(course_id);
CREATE INDEX IF NOT EXISTS ix_sub_assignment ON Submissions(assignment_id, status);
CREATE INDEX IF NOT EXISTS ix_marks_due      ON Marks(review_due_at, released_at);
CREATE INDEX IF NOT EXISTS ix_audit_mark     ON AuditLog(mark_id);
 
-- Reserved account for automatic actions; the hash can never verify, so nobody can log in as SYSTEM
INSERT OR IGNORE INTO Users(user_id, username, password_hash, full_name, role, is_active)
VALUES (1, 'SYSTEM', '!', 'System (automatic actions)', 'Admin', 0);)LMSSQL"; }
