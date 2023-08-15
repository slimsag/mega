// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/** Coordinates the views/mediators that make up the bookmark folder picker. */
public class BookmarkFolderPickerCoordinator implements BackPressHandler {
    private BookmarkUiPrefs.Observer mBookmarkUiPrefsObserver = new BookmarkUiPrefs.Observer() {
        @Override
        @SuppressWarnings("NotifyDataSetChanged")
        public void onBookmarkRowDisplayPrefChanged(@BookmarkRowDisplayPref int displayPref) {
            if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
                mAdapter.notifyDataSetChanged();
            }
        }
    };

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private final ModelList mModelList = new ModelList();
    private final SelectionDelegate mEmptySelectionDelegate = new SelectionDelegate();
    private final Context mContext;
    private final View mView;
    private final View mMoveButton;
    private final RecyclerView mRecyclerView;
    private final BookmarkFolderPickerMediator mMediator;
    private final BookmarkUiPrefs mBookmarkUiPrefs;

    private final SimpleRecyclerViewAdapter mAdapter = new SimpleRecyclerViewAdapter(mModelList) {
        @Override
        public void onViewRecycled(SimpleRecyclerViewAdapter.ViewHolder holder) {
            super.onViewRecycled(holder);
            if (holder == null || holder.model == null) return;
            holder.model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR).setView(null);
        }
    };

    public BookmarkFolderPickerCoordinator(Context context, BookmarkModel bookmarkModel,
            BookmarkImageFetcher bookmarkImageFetcher, List<BookmarkId> bookmarkIds,
            BookmarkId initialParentId, Runnable finishRunnable,
            BookmarkAddNewFolderCoordinator addNewFolderCoordinator,
            BookmarkUiPrefs bookmarkUiPrefs,
            ImprovedBookmarkRowCoordinator improvedBookmarkRowCoordinator) {
        mContext = context;
        mView = LayoutInflater.from(mContext).inflate(R.layout.bookmark_folder_picker, null);
        mMoveButton = mView.findViewById(R.id.move_button);

        mRecyclerView = mView.findViewById(R.id.folder_recycler_view);
        mRecyclerView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
        mRecyclerView.setAdapter(mAdapter);
        mAdapter.registerType(BookmarkFolderPickerMediator.FOLDER_ROW, this::buildFolderRow,
                ImprovedBookmarkRowViewBinder::bind);

        PropertyModel model = new PropertyModel(BookmarkFolderPickerProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(model, mView, BookmarkFolderPickerViewBinder::bind);

        mMediator = new BookmarkFolderPickerMediator(context, bookmarkModel, bookmarkImageFetcher,
                bookmarkIds, initialParentId, finishRunnable,
                new BookmarkUiPrefs(SharedPreferencesManager.getInstance()), model, mModelList,
                addNewFolderCoordinator, improvedBookmarkRowCoordinator);

        FadingShadowView shadow = (FadingShadowView) mView.findViewById(R.id.shadow);
        shadow.init(mContext.getColor(R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);
        mRecyclerView.setOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                super.onScrollStateChanged(recyclerView, newState);
            }

            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                super.onScrolled(recyclerView, dx, dy);
                shadow.setVisibility(
                        mRecyclerView.canScrollVertically(-1) ? View.VISIBLE : View.GONE);
            }
        });

        // Back presses are always handled.
        mBackPressStateSupplier.set(true);

        mBookmarkUiPrefs = bookmarkUiPrefs;
        mBookmarkUiPrefs.addObserver(mBookmarkUiPrefsObserver);
    }

    /** Destroys the coordinator. */
    public void destroy() {
        mMediator.destroy();
        mBookmarkUiPrefs.removeObserver(mBookmarkUiPrefsObserver);
    }

    /** Returns the view for display. */
    public View getView() {
        return mView;
    }

    /** Returns the {@link Toolbar} for the folder picker. */
    public Toolbar getToolbar() {
        return (Toolbar) mView.findViewById(R.id.toolbar);
    }

    public void updateToolbarButtons() {
        mMediator.updateToolbarButtons();
    }

    // Delegate setup methods.

    /** Handle option menu selections. */
    public boolean optionsItemSelected(MenuItem item) {
        return mMediator.optionsItemSelected(item.getItemId());
    }

    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    // Building rows for the recycler view.

    View buildFolderRow(ViewGroup parent) {
        ImprovedBookmarkRow row = ImprovedBookmarkRow.buildView(parent.getContext(),
                mBookmarkUiPrefs.getBookmarkRowDisplayPref() == BookmarkRowDisplayPref.VISUAL);
        row.setSelectionDelegate(mEmptySelectionDelegate);
        return row;
    }

    // BackPressHandler implementation.

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    // Testing methods.

    void openFolderForTesting(BookmarkId folder) {
        mMediator.populateFoldersForParentId(folder);
    }
}
