package org.mahoyo.port;

import android.content.*;
import android.database.*;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.*;

/** A single read-only, URI-granted APK. No directory browsing or arbitrary paths. */
public final class UpdateApkProvider extends ContentProvider {
    static File file(Context context){return new File(context.getFilesDir(),"updates/release.apk");}
    private File checked(Uri uri)throws FileNotFoundException {if(!"/release.apk".equals(uri.getPath())||uri.getQuery()!=null||uri.getFragment()!=null||!file(getContext()).isFile())throw new FileNotFoundException();return file(getContext());}
    @Override public boolean onCreate(){return true;}
    @Override public String getType(Uri uri){return "application/vnd.android.package-archive";}
    @Override public ParcelFileDescriptor openFile(Uri uri,String mode)throws FileNotFoundException {if(!mode.equals("r"))throw new FileNotFoundException("Read only");return ParcelFileDescriptor.open(checked(uri),ParcelFileDescriptor.MODE_READ_ONLY);}
    @Override public Cursor query(Uri uri,String[] projection,String selection,String[] args,String order){try{File file=checked(uri);String[] columns=projection==null?new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE}:projection;MatrixCursor cursor=new MatrixCursor(columns);Object[] values=new Object[columns.length];for(int i=0;i<columns.length;i++)values[i]=columns[i].equals(OpenableColumns.DISPLAY_NAME)?"mahoyo-unofficial.apk":columns[i].equals(OpenableColumns.SIZE)?file.length():null;cursor.addRow(values);return cursor;}catch(FileNotFoundException error){return null;}}
    @Override public Uri insert(Uri uri,ContentValues values){throw new UnsupportedOperationException();}
    @Override public int update(Uri uri,ContentValues values,String selection,String[] args){throw new UnsupportedOperationException();}
    @Override public int delete(Uri uri,String selection,String[] args){throw new UnsupportedOperationException();}
}
