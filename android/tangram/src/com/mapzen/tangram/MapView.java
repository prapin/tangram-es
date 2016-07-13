package com.mapzen.tangram;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.widget.FrameLayout;

/**
 * {@code MapView} is a View for displaying a Tangram map.
 */
public class MapView extends FrameLayout {

    protected GLSurfaceView glSurfaceView;
    protected MapController mapController;
    protected AsyncTask<Void, Void, Boolean> getMapTask;

    public MapView(Context context) {

        super(context);

        configureGLSurfaceView();

    }

    public MapView(Context context, AttributeSet attrs) {

        super(context, attrs);

        configureGLSurfaceView();

    }

    /**
     * Interface for receiving a {@code MapController} once it is ready to be used
     */
    public interface OnMapReadyCallback {

        /**
         * Called when the map is ready to be used
         * @param mapController A non-null {@code MapController} instance for this {@code MapView}
         */
        void onMapReady(MapController mapController);

    }

    /**
     * Construct a {@code MapController} asynchronously; may only be called from the UI thread
     * @param callback The object to receive the resulting MapController in a callback;
     * the callback will be made on the UI thread
     * @param sceneFilePath Location of the YAML scene file within the asset bundle
     */
    public void getMapAsync(@NonNull final OnMapReadyCallback callback,
                            @NonNull final String sceneFilePath) {

        disposeMap();

        mapController = getMapInstance(sceneFilePath);

        getMapTask = new AsyncTask<Void, Void, Boolean>() {

            @Override
            @SuppressWarnings("WrongThread")
            protected Boolean doInBackground(Void... params) {
                mapController.init();
                mapController.loadSceneFile(sceneFilePath);
                return true;
            }

            @Override
            protected void onPostExecute(Boolean ok) {
                addView(glSurfaceView);
                callback.onMapReady(mapController);
            }

            @Override
            protected void onCancelled(Boolean ok) {
                mapController.dispose();
            }

        }.execute();

    }

    protected MapController getMapInstance(String sceneFilePath) {
        return MapController.getInstance(glSurfaceView, sceneFilePath);
    }

    protected void configureGLSurfaceView() {

        glSurfaceView = new GLSurfaceView(getContext());
        glSurfaceView.setEGLContextClientVersion(2);
        glSurfaceView.setPreserveEGLContextOnPause(true);
        glSurfaceView.setEGLConfigChooser(new ConfigChooser(8, 8, 8, 0, 16, 0));

    }

    protected void disposeMap() {

        if (getMapTask != null) {
            // MapController is being initialized, so we'll dispose it in the onCancelled callback.
            getMapTask.cancel(true);
        } else if (mapController != null) {
            // MapController has been initialized, so we'll dispose it now on the GL thread.
            mapController.queueEvent(new Runnable() {
                @Override
                public void run() {
                    mapController.dispose();
                }
            });
        }
        mapController = null;
        getMapTask = null;

    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onCreate(Bundle savedInstanceState) {

    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onResume() {

    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onPause() {

    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onDestroy() {

        disposeMap();

    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onLowMemory() {

    }

}
