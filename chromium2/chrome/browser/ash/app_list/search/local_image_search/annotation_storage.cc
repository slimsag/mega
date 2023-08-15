// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.h"
#include "chrome/browser/ash/app_list/search/local_image_search/search_utils.h"
#include "chrome/browser/ash/app_list/search/local_image_search/sql_database.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "sql/statement.h"

namespace app_list {
namespace {

using FuzzyTokenizedStringMatch =
    ::ash::string_matching::FuzzyTokenizedStringMatch;
using TokenizedString = ::ash::string_matching::TokenizedString;
using Mode = ::ash::string_matching::TokenizedString::Mode;

constexpr double kRelevanceThreshold = 0.79;
constexpr int kVersionNumber = 3;

// Initializes a new annotation table, returning a schema version number
// on success. The table can be searched by label and image path.
// The map between label and image is many-to-one.
// The table cannot exist when calling this function.
int CreateNewSchema(SqlDatabase* db) {
  DVLOG(1) << "Making a table";
  if (!db) {
    return 0;
  }

  static constexpr char kQuery[] =
      // clang-format off
      "CREATE TABLE annotations("
          "label TEXT NOT NULL,"
          "image_path TEXT NOT NULL,"
          "last_modified_time INTEGER NOT NULL,"
          "is_ignored INTEGER NOT NULL)";
  // clang-format on
  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement || !statement->Run()) {
    return 0;
  }

  static constexpr char kQuery1[] =
      "CREATE INDEX ind_annotations_label ON annotations(label)";

  std::unique_ptr<sql::Statement> statement1 =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery1);
  if (!statement1 || !statement1->Run()) {
    return 0;
  }

  static constexpr char kQuery2[] =
      "CREATE INDEX ind_annotations_image_path ON annotations(image_path)";

  std::unique_ptr<sql::Statement> statement2 =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery2);
  if (!statement2 || !statement2->Run()) {
    return 0;
  }

  return kVersionNumber;
}

int MigrateSchema(SqlDatabase* db, int current_version_number) {
  if (!db) {
    return 0;
  }

  if (current_version_number == kVersionNumber) {
    return current_version_number;
  }

  static constexpr char kQuery[] = "DROP TABLE IF EXISTS annotations";
  std::unique_ptr<sql::Statement> statement =
      db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement || !statement->Run()) {
    return 0;
  }

  return CreateNewSchema(db);
}

// Returns sorted `FileSearchResult`s contained in both sorted arrays.
std::vector<FileSearchResult> FindIntersection(
    const std::vector<FileSearchResult>& vec1,
    const std::vector<FileSearchResult>& vec2) {
  std::vector<FileSearchResult> result;

  auto it1 = vec1.begin();
  auto it2 = vec2.begin();

  while (it1 != vec1.end() && it2 != vec2.end()) {
    if (it1->file_path < it2->file_path) {
      ++it1;
    } else if (it2->file_path < it1->file_path) {
      ++it2;
    } else {
      result.emplace_back(FileSearchResult(it1->file_path, it1->last_modified,
                                           it1->relevance + it2->relevance));
      ++it1;
      ++it2;
    }
  }

  return result;
}

}  // namespace

ImageInfo::ImageInfo(const std::set<std::string>& annotations,
                     const base::FilePath& path,
                     const base::Time& last_modified,
                     bool is_ignored)
    : annotations(annotations),
      path(path),
      last_modified(last_modified),
      is_ignored(is_ignored) {}

ImageInfo::~ImageInfo() = default;
ImageInfo::ImageInfo(const ImageInfo&) = default;

FileSearchResult::FileSearchResult(const base::FilePath& file_path,
                                   const base::Time& last_modified,
                                   double relevance)
    : file_path(file_path),
      last_modified(last_modified),
      relevance(relevance) {}

FileSearchResult::~FileSearchResult() = default;
FileSearchResult::FileSearchResult(const FileSearchResult&) = default;
FileSearchResult& FileSearchResult::operator=(const FileSearchResult&) =
    default;

AnnotationStorage::AnnotationStorage(
    const base::FilePath& path_to_db,
    const std::string& histogram_tag,
    int current_version_number,
    std::unique_ptr<ImageAnnotationWorker> annotation_worker)
    : annotation_worker_(std::move(annotation_worker)),
      sql_database_(
          std::make_unique<SqlDatabase>(path_to_db,
                                        histogram_tag,
                                        current_version_number,
                                        base::BindRepeating(CreateNewSchema),
                                        base::BindRepeating(MigrateSchema))) {
  DVLOG(1) << "Construct AnnotationStorage";
}

AnnotationStorage::AnnotationStorage(
    const base::FilePath& path_to_db,
    const std::string& histogram_tag,
    std::unique_ptr<ImageAnnotationWorker> annotation_worker)
    : AnnotationStorage(path_to_db,
                        histogram_tag,
                        kVersionNumber,
                        std::move(annotation_worker)) {}

AnnotationStorage::~AnnotationStorage() = default;

void AnnotationStorage::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sql_database_->Initialize()) {
    LOG(ERROR) << "Failed to initialize the db.";
    return;
  }
  if (annotation_worker_ != nullptr) {
    // Owns `annotation_worker_`.
    annotation_worker_->Initialize(this);
  }
}

void AnnotationStorage::Insert(const ImageInfo& image_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Insert";

  static constexpr char kQuery[] =
      // clang-format off
      "INSERT INTO annotations(label,image_path,last_modified_time,is_ignored) "
          "VALUES(?,?,?,?)";
  // clang-format on

  for (const auto& annotation : image_info.annotations) {
    std::unique_ptr<sql::Statement> statement =
        sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
    if (!statement) {
      return;
    }
    DVLOG(1) << annotation;
    statement->BindString(0, annotation);
    statement->BindString(1, image_info.path.value());
    statement->BindTime(2, image_info.last_modified);
    statement->BindInt(3, image_info.is_ignored);

    if (!statement->Run()) {
      // TODO(b/260646344): log to UMA instead.
      return;
    }
  }
  return;
}

void AnnotationStorage::Remove(const base::FilePath& image_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Remove";

  static constexpr char kQuery[] = "DELETE FROM annotations WHERE image_path=?";

  std::unique_ptr<sql::Statement> statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    return;
  }

  statement->BindString(0, image_path.value());

  statement->Run();
}

std::vector<ImageInfo> AnnotationStorage::GetAllAnnotations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "GetAllAnnotations";

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT label,image_path,last_modified_time,is_ignored "
          "FROM annotations "
          "ORDER BY label";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    return {};
  }

  std::vector<ImageInfo> matched_paths;
  while (statement->Step()) {
    const base::FilePath path = base::FilePath(statement->ColumnString(1));
    const base::Time time = statement->ColumnTime(2);
    const bool is_ignored = statement->ColumnBool(3);
    DVLOG(1) << "Select find: " << statement->ColumnString(0) << ", " << path
             << ", " << time;
    matched_paths.push_back({{statement->ColumnString(0)},
                             std::move(path),
                             std::move(time),
                             is_ignored});
  }

  return matched_paths;
}

std::vector<ImageInfo> AnnotationStorage::FindImagePath(
    const base::FilePath& image_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!image_path.empty());
  DVLOG(1) << "FindImagePath " << image_path;

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT label,image_path,last_modified_time,is_ignored "
          "FROM annotations "
          "WHERE image_path=? "
          "ORDER BY label";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    return {};
  }
  statement->BindString(0, image_path.value());

  std::vector<ImageInfo> matched_paths;
  while (statement->Step()) {
    const base::FilePath path = base::FilePath(statement->ColumnString(1));
    const base::Time time = statement->ColumnTime(2);
    const bool is_ignored = statement->ColumnBool(3);
    DVLOG(1) << "Select find: " << statement->ColumnString(0) << ", " << path
             << ", " << time;
    matched_paths.push_back({{statement->ColumnString(0)},
                             std::move(path),
                             std::move(time),
                             is_ignored});
  }

  return matched_paths;
}

std::vector<FileSearchResult> AnnotationStorage::PrefixSearch(
    const std::u16string& query_term) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "PrefixSearch " << query_term;

  // LIKE is 10 times faster than the linear search.
  static constexpr char kQuery[] =
      // clang-format off
      "SELECT label,image_path,last_modified_time,is_ignored "
          "FROM annotations "
          "WHERE is_ignored=0 "
          "AND label LIKE ? "
          "ORDER BY image_path";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    return {};
  }
  statement->BindString(0, base::StrCat({base::UTF16ToUTF8(query_term), "%"}));

  std::vector<FileSearchResult> matched_paths;
  TokenizedString tokenized_query(query_term, Mode::kWords);
  while (statement->Step()) {
    double relevance = FuzzyTokenizedStringMatch::TokenSetRatio(
        tokenized_query,
        TokenizedString(base::UTF8ToUTF16(statement->ColumnString(0)),
                        Mode::kWords),
        /*partial=*/false);
    if (relevance < kRelevanceThreshold) {
      continue;
    }

    const base::FilePath path = base::FilePath(statement->ColumnString(1));
    const base::Time time = statement->ColumnTime(2);
    DVLOG(1) << "Select: " << statement->ColumnString(0) << ", " << path << ", "
             << time << " rl: " << relevance;

    if (matched_paths.empty() || matched_paths.back().file_path != path) {
      matched_paths.push_back({path, std::move(time), relevance});
    } else if (matched_paths.back().relevance < relevance) {
      matched_paths.back().relevance = relevance;
    }
  }
  return matched_paths;
}

std::vector<FileSearchResult> AnnotationStorage::Search(
    const std::u16string& query,
    size_t max_num_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (max_num_results < 1) {
    return {};
  }

  TokenizedString tokenized_query(query, Mode::kWords);
  if (tokenized_query.tokens().empty()) {
    return {};
  }

  std::vector<FileSearchResult> results;
  int normalization_constant = tokenized_query.tokens().size();
  bool fist_result = true;
  for (const auto& token : tokenized_query.tokens()) {
    if (IsStopWord(base::UTF16ToUTF8(token))) {
      normalization_constant -= 1;
      continue;
    }

    std::vector<FileSearchResult> next_result = PrefixSearch(token);
    if (next_result.empty()) {
      return {};
    }
    results =
        (fist_result) ? next_result : FindIntersection(results, next_result);
    fist_result = false;
  }

  if (results.size() <= max_num_results) {
    std::sort(results.begin(), results.end(),
              [](const FileSearchResult& a, const FileSearchResult& b) {
                return a.relevance > b.relevance;
              });
  } else {
    std::partial_sort(results.begin(), results.begin() + max_num_results,
                      results.end(),
                      [](const FileSearchResult& a, const FileSearchResult& b) {
                        return a.relevance > b.relevance;
                      });
    results = std::vector<FileSearchResult>(results.begin(),
                                            results.begin() + max_num_results);
  }

  // Normalize to [0, 1].
  for (auto& result : results) {
    result.relevance = result.relevance / normalization_constant;
  }

  return results;
}

}  // namespace app_list
