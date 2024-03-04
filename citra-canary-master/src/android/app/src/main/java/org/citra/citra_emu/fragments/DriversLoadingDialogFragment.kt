// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.fragments

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.launch
import org.citra.citra_emu.R
import org.citra.citra_emu.databinding.DialogProgressBarBinding
import org.citra.citra_emu.viewmodel.DriverViewModel

class DriversLoadingDialogFragment : DialogFragment() {
    private val driverViewModel: DriverViewModel by activityViewModels()

    private lateinit var binding: DialogProgressBarBinding

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        binding = DialogProgressBarBinding.inflate(layoutInflater)
        binding.progressBar.isIndeterminate = true

        isCancelable = false

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.loading)
            .setView(binding.root)
            .create()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View = binding.root

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.RESUMED) {
                    driverViewModel.areDriversLoading.collect { checkForDismiss() }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.RESUMED) {
                    driverViewModel.isDriverReady.collect { checkForDismiss() }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.RESUMED) {
                    driverViewModel.isDeletingDrivers.collect { checkForDismiss() }
                }
            }
        }
    }

    private fun checkForDismiss() {
        if (driverViewModel.isInteractionAllowed) {
            dismiss()
        }
    }

    companion object {
        const val TAG = "DriversLoadingDialogFragment"
    }
}
