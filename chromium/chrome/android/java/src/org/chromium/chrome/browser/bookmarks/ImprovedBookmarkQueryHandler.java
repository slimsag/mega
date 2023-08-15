// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.power_bookmarks.PowerBookmarkType;

import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/** New implementation of {@link BookmarkQueryHandler} that expands the root. */
public class ImprovedBookmarkQueryHandler implements BookmarkQueryHandler {
    private final BookmarkModel mBookmarkModel;
    private final BasicBookmarkQueryHandler mBasicBookmarkQueryHandler;
    private final BookmarkUiPrefs mBookmarkUiPrefs;

    /**
     * Constructs a handle that operates on the given backend.
     * @param bookmarkModel The backend that holds the truth of what the bookmark state looks like.
     * @param bookmarkUiPrefs Stores the display prefs for bookmarks.
     */
    public ImprovedBookmarkQueryHandler(
            BookmarkModel bookmarkModel, BookmarkUiPrefs bookmarkUiPrefs) {
        mBookmarkModel = bookmarkModel;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mBasicBookmarkQueryHandler = new BasicBookmarkQueryHandler(bookmarkModel, mBookmarkUiPrefs);
    }

    @Override
    public void destroy() {
        mBasicBookmarkQueryHandler.destroy();
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForParent(BookmarkId parentId) {
        final List<BookmarkListEntry> bookmarkListEntries;
        bookmarkListEntries = mBasicBookmarkQueryHandler.buildBookmarkListForParent(parentId);

        // Don't do anything for ReadingList, they're already sorted with a different mechanism.
        if (!Objects.equals(parentId, mBookmarkModel.getReadingListFolder())) {
            sortByStoredPref(bookmarkListEntries);
        }

        return bookmarkListEntries;
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForSearch(
            String query, Set<PowerBookmarkType> powerFilter) {
        List<BookmarkListEntry> bookmarkListEntries =
                mBasicBookmarkQueryHandler.buildBookmarkListForSearch(query, powerFilter);
        sortByStoredPref(bookmarkListEntries);
        return bookmarkListEntries;
    }

    private void sortByStoredPref(List<BookmarkListEntry> bookmarkListEntries) {
        final @BookmarkRowSortOrder int sortOrder = mBookmarkUiPrefs.getBookmarkRowSortOrder();
        Collections.sort(
                bookmarkListEntries, (BookmarkListEntry entry1, BookmarkListEntry entry2) -> {
                    BookmarkItem item1 = entry1.getBookmarkItem();
                    BookmarkItem item2 = entry2.getBookmarkItem();

                    // Sort folders before urls.
                    int folderComparison = Boolean.compare(item2.isFolder(), item1.isFolder());
                    if (folderComparison != 0) {
                        return folderComparison;
                    }

                    int titleComparison = sortCompare(item1, item2, sortOrder);
                    if (titleComparison != 0) {
                        return titleComparison;
                    }

                    // Fall back to id in case other fields tie. Order will be arbitrary but
                    // consistent.
                    return Long.compare(item1.getId().getId(), item2.getId().getId());
                });
    }

    private int sortCompare(
            BookmarkItem item1, BookmarkItem item2, @BookmarkRowSortOrder int sortOrder) {
        switch (sortOrder) {
            case BookmarkRowSortOrder.CHRONOLOGICAL:
                return Long.compare(item1.getDateAdded(), item2.getDateAdded());
            case BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL:
                return Long.compare(item2.getDateAdded(), item1.getDateAdded());
            case BookmarkRowSortOrder.ALPHABETICAL:
                return item1.getTitle().compareToIgnoreCase(item2.getTitle());
            case BookmarkRowSortOrder.REVERSE_ALPHABETICAL:
                return item2.getTitle().compareToIgnoreCase(item1.getTitle());
            case BookmarkRowSortOrder.RECENTLY_USED:
                return Long.compare(item2.getDateLastOpened(), item1.getDateLastOpened());
        }
        return 0;
    }
}
