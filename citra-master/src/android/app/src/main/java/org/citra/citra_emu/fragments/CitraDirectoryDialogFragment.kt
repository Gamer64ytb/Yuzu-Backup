// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentActivity
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.citra.citra_emu.R
import org.citra.citra_emu.databinding.DialogCitraDirectoryBinding
import org.citra.citra_emu.ui.main.MainActivity
import org.citra.citra_emu.utils.PermissionsHandler
import org.citra.citra_emu.viewmodel.HomeViewModel

class CitraDirectoryDialogFragment : DialogFragment() {
    private lateinit var binding: DialogCitraDirectoryBinding

    private val homeViewModel: HomeViewModel by activityViewModels()

    fun interface Listener {
        fun onPressPositiveButton(moveData: Boolean, path: Uri)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        binding = DialogCitraDirectoryBinding.inflate(layoutInflater)

        val path = Uri.parse(requireArguments().getString(PATH))

        binding.checkBox.isChecked = savedInstanceState?.getBoolean(MOVE_DATE_ENABLE) ?: false
        val oldPath = PermissionsHandler.citraDirectory
        if (!PermissionsHandler.hasWriteAccess(requireActivity()) ||
            oldPath.toString() == path.toString()
        ) {
            binding.checkBox.visibility = View.GONE
        }
        binding.path.text = path.path
        binding.path.isSelected = true

        isCancelable = false
        return MaterialAlertDialogBuilder(requireContext())
            .setView(binding.root)
            .setTitle(R.string.select_citra_user_folder)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface?, _: Int ->
                homeViewModel.directoryListener?.onPressPositiveButton(
                    if (binding.checkBox.visibility != View.GONE) {
                        binding.checkBox.isChecked
                    } else {
                        false
                    },
                    path
                )
            }
            .setNegativeButton(android.R.string.cancel) { _: DialogInterface?, _: Int ->
                if (!PermissionsHandler.hasWriteAccess(requireContext())) {
                    (requireActivity() as MainActivity).openCitraDirectory.launch(null)
                }
            }
            .show()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putBoolean(MOVE_DATE_ENABLE, binding.checkBox.isChecked)
    }

    companion object {
        const val TAG = "citra_directory_dialog_fragment"
        private const val MOVE_DATE_ENABLE = "IS_MODE_DATA_ENABLE"
        private const val PATH = "path"

        fun newInstance(
            activity: FragmentActivity,
            path: String,
            listener: Listener
        ): CitraDirectoryDialogFragment {
            val dialog = CitraDirectoryDialogFragment()
            ViewModelProvider(activity)[HomeViewModel::class.java].directoryListener = listener
            val args = Bundle()
            args.putString(PATH, path)
            dialog.arguments = args
            return dialog
        }
    }
}
