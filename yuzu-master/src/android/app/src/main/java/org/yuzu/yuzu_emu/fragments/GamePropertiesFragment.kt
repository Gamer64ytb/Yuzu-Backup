// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.HomeNavigationDirections
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.adapters.GamePropertiesAdapter
import org.yuzu.yuzu_emu.databinding.FragmentGamePropertiesBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.GameProperty
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.model.InstallableProperty
import org.yuzu.yuzu_emu.model.SubmenuProperty
import org.yuzu.yuzu_emu.model.TaskState
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GameIconUtils
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import org.yuzu.yuzu_emu.utils.MemoryUtil
import org.yuzu.yuzu_emu.utils.ViewUtils.marquee
import org.yuzu.yuzu_emu.utils.ViewUtils.updateMargins
import org.yuzu.yuzu_emu.utils.collect
import java.io.BufferedOutputStream
import java.io.File

class GamePropertiesFragment : Fragment() {
    private var _binding: FragmentGamePropertiesBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()
    private val gamesViewModel: GamesViewModel by activityViewModels()
    private val driverViewModel: DriverViewModel by activityViewModels()

    private val args by navArgs<GamePropertiesFragmentArgs>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.Y, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.Y, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentGamePropertiesBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        homeViewModel.setNavigationVisibility(visible = false, animated = true)
        homeViewModel.setStatusBarShadeVisibility(true)

        binding.buttonBack.setOnClickListener {
            view.findNavController().popBackStack()
        }

        val shortcutManager = requireActivity().getSystemService(ShortcutManager::class.java)
        binding.buttonShortcut.isEnabled = shortcutManager.isRequestPinShortcutSupported
        binding.buttonShortcut.setOnClickListener {
            viewLifecycleOwner.lifecycleScope.launch {
                withContext(Dispatchers.IO) {
                    val shortcut = ShortcutInfo.Builder(requireContext(), args.game.title)
                        .setShortLabel(args.game.title)
                        .setIcon(
                            GameIconUtils.getShortcutIcon(requireActivity(), args.game)
                                .toIcon(requireContext())
                        )
                        .setIntent(args.game.launchIntent)
                        .build()
                    shortcutManager.requestPinShortcut(shortcut, null)
                }
            }
        }

        GameIconUtils.loadGameIcon(args.game, binding.imageGameScreen)
        binding.title.text = args.game.title
        binding.title.marquee()

        binding.buttonStart.setOnClickListener {
            LaunchGameDialogFragment.newInstance(args.game)
                .show(childFragmentManager, LaunchGameDialogFragment.TAG)
        }

        reloadList()

        homeViewModel.openImportSaves.collect(
            viewLifecycleOwner,
            resetState = { homeViewModel.setOpenImportSaves(false) }
        ) { if (it) importSaves.launch(arrayOf("application/zip")) }
        homeViewModel.reloadPropertiesList.collect(
            viewLifecycleOwner,
            resetState = { homeViewModel.reloadPropertiesList(false) }
        ) { if (it) reloadList() }

        setInsets()
    }

    override fun onDestroy() {
        super.onDestroy()
        gamesViewModel.reloadGames(true)
    }

    private fun reloadList() {
        _binding ?: return

        driverViewModel.updateDriverNameForGame(args.game)
        val properties = mutableListOf<GameProperty>().apply {
            add(
                SubmenuProperty(
                    R.string.info,
                    R.string.info_description,
                    R.drawable.ic_info_outline
                ) {
                    val action = GamePropertiesFragmentDirections
                        .actionPerGamePropertiesFragmentToGameInfoFragment(args.game)
                    binding.root.findNavController().navigate(action)
                }
            )
            add(
                SubmenuProperty(
                    R.string.preferences_settings,
                    R.string.per_game_settings_description,
                    R.drawable.ic_settings
                ) {
                    val action = HomeNavigationDirections.actionGlobalSettingsActivity(
                        args.game,
                        Settings.MenuTag.SECTION_ROOT
                    )
                    binding.root.findNavController().navigate(action)
                }
            )

            if (GpuDriverHelper.supportsCustomDriverLoading()) {
                add(
                    SubmenuProperty(
                        R.string.gpu_driver_manager,
                        R.string.install_gpu_driver_description,
                        R.drawable.ic_build,
                        detailsFlow = driverViewModel.selectedDriverTitle
                    ) {
                        val action = GamePropertiesFragmentDirections
                            .actionPerGamePropertiesFragmentToDriverManagerFragment(args.game)
                        binding.root.findNavController().navigate(action)
                    }
                )
            }

            if (!args.game.isHomebrew) {
                add(
                    SubmenuProperty(
                        R.string.add_ons,
                        R.string.add_ons_description,
                        R.drawable.ic_edit
                    ) {
                        val action = GamePropertiesFragmentDirections
                            .actionPerGamePropertiesFragmentToAddonsFragment(args.game)
                        binding.root.findNavController().navigate(action)
                    }
                )
                add(
                    InstallableProperty(
                        R.string.save_data,
                        R.string.save_data_description,
                        R.drawable.ic_save,
                        {
                            MessageDialogFragment.newInstance(
                                requireActivity(),
                                titleId = R.string.import_save_warning,
                                descriptionId = R.string.import_save_warning_description,
                                positiveAction = { homeViewModel.setOpenImportSaves(true) }
                            ).show(parentFragmentManager, MessageDialogFragment.TAG)
                        },
                        if (File(args.game.saveDir).exists()) {
                            { exportSaves.launch(args.game.saveZipName) }
                        } else {
                            null
                        }
                    )
                )

                val saveDirFile = File(args.game.saveDir)
                if (saveDirFile.exists()) {
                    add(
                        SubmenuProperty(
                            R.string.delete_save_data,
                            R.string.delete_save_data_description,
                            R.drawable.ic_delete,
                            action = {
                                MessageDialogFragment.newInstance(
                                    requireActivity(),
                                    titleId = R.string.delete_save_data,
                                    descriptionId = R.string.delete_save_data_warning_description,
                                    positiveButtonTitleId = android.R.string.cancel,
                                    negativeButtonTitleId = android.R.string.ok,
                                    negativeAction = {
                                        File(args.game.saveDir).deleteRecursively()
                                        Toast.makeText(
                                            YuzuApplication.appContext,
                                            R.string.save_data_deleted_successfully,
                                            Toast.LENGTH_SHORT
                                        ).show()
                                        homeViewModel.reloadPropertiesList(true)
                                    }
                                ).show(parentFragmentManager, MessageDialogFragment.TAG)
                            }
                        )
                    )
                }

                val shaderCacheDir = File(
                    DirectoryInitialization.userDirectory +
                        "/shader/" + args.game.settingsName.lowercase()
                )
                if (shaderCacheDir.exists()) {
                    add(
                        SubmenuProperty(
                            R.string.clear_shader_cache,
                            R.string.clear_shader_cache_description,
                            R.drawable.ic_delete,
                            {
                                if (shaderCacheDir.exists()) {
                                    val bytes = shaderCacheDir.walkTopDown().filter { it.isFile }
                                        .map { it.length() }.sum()
                                    MemoryUtil.bytesToSizeUnit(bytes.toFloat())
                                } else {
                                    MemoryUtil.bytesToSizeUnit(0f)
                                }
                            }
                        ) {
                            MessageDialogFragment.newInstance(
                                requireActivity(),
                                titleId = R.string.clear_shader_cache,
                                descriptionId = R.string.clear_shader_cache_warning_description,
                                positiveAction = {
                                    shaderCacheDir.deleteRecursively()
                                    Toast.makeText(
                                        YuzuApplication.appContext,
                                        R.string.cleared_shaders_successfully,
                                        Toast.LENGTH_SHORT
                                    ).show()
                                    homeViewModel.reloadPropertiesList(true)
                                }
                            ).show(parentFragmentManager, MessageDialogFragment.TAG)
                        }
                    )
                }
            }
        }
        binding.listProperties.apply {
            layoutManager =
                GridLayoutManager(requireContext(), resources.getInteger(R.integer.grid_columns))
            adapter = GamePropertiesAdapter(viewLifecycleOwner, properties)
        }
    }

    override fun onResume() {
        super.onResume()
        driverViewModel.updateDriverNameForGame(args.game)
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val smallLayout = resources.getBoolean(R.bool.small_layout)
            if (smallLayout) {
                binding.listAll.updateMargins(left = leftInsets, right = rightInsets)
            } else {
                if (ViewCompat.getLayoutDirection(binding.root) ==
                    ViewCompat.LAYOUT_DIRECTION_LTR
                ) {
                    binding.listAll.updateMargins(right = rightInsets)
                    binding.iconLayout!!.updateMargins(top = barInsets.top, left = leftInsets)
                } else {
                    binding.listAll.updateMargins(left = leftInsets)
                    binding.iconLayout!!.updateMargins(top = barInsets.top, right = rightInsets)
                }
            }

            val fabSpacing = resources.getDimensionPixelSize(R.dimen.spacing_fab)
            binding.buttonStart.updateMargins(
                left = leftInsets + fabSpacing,
                right = rightInsets + fabSpacing,
                bottom = barInsets.bottom + fabSpacing
            )

            binding.layoutAll.updatePadding(
                top = barInsets.top,
                bottom = barInsets.bottom +
                    resources.getDimensionPixelSize(R.dimen.spacing_bottom_list_fab)
            )

            windowInsets
        }

    private val importSaves =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null) {
                return@registerForActivityResult
            }

            val savesFolder = File(args.game.saveDir)
            val cacheSaveDir = File("${requireContext().cacheDir.path}/saves/")
            cacheSaveDir.mkdir()

            ProgressDialogFragment.newInstance(
                requireActivity(),
                R.string.save_files_importing,
                false
            ) { _, _ ->
                try {
                    FileUtil.unzipToInternalStorage(result.toString(), cacheSaveDir)
                    val files = cacheSaveDir.listFiles()
                    var savesFolderFile: File? = null
                    if (files != null) {
                        val savesFolderName = args.game.programIdHex
                        for (file in files) {
                            if (file.isDirectory && file.name == savesFolderName) {
                                savesFolderFile = file
                                break
                            }
                        }
                    }

                    if (savesFolderFile != null) {
                        savesFolder.deleteRecursively()
                        savesFolder.mkdir()
                        savesFolderFile.copyRecursively(savesFolder)
                        savesFolderFile.deleteRecursively()
                    }

                    withContext(Dispatchers.Main) {
                        if (savesFolderFile == null) {
                            MessageDialogFragment.newInstance(
                                requireActivity(),
                                titleId = R.string.save_file_invalid_zip_structure,
                                descriptionId = R.string.save_file_invalid_zip_structure_description
                            ).show(parentFragmentManager, MessageDialogFragment.TAG)
                            return@withContext
                        }
                        Toast.makeText(
                            YuzuApplication.appContext,
                            getString(R.string.save_file_imported_success),
                            Toast.LENGTH_LONG
                        ).show()
                        homeViewModel.reloadPropertiesList(true)
                    }

                    cacheSaveDir.deleteRecursively()
                } catch (e: Exception) {
                    Toast.makeText(
                        YuzuApplication.appContext,
                        getString(R.string.fatal_error),
                        Toast.LENGTH_LONG
                    ).show()
                }
            }.show(parentFragmentManager, ProgressDialogFragment.TAG)
        }

    /**
     * Exports the save file located in the given folder path by creating a zip file and opening a
     * file picker to save.
     */
    private val exportSaves = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/zip")
    ) { result ->
        if (result == null) {
            return@registerForActivityResult
        }

        ProgressDialogFragment.newInstance(
            requireActivity(),
            R.string.save_files_exporting,
            false
        ) { _, _ ->
            val saveLocation = args.game.saveDir
            val zipResult = FileUtil.zipFromInternalStorage(
                File(saveLocation),
                saveLocation.replaceAfterLast("/", ""),
                BufferedOutputStream(requireContext().contentResolver.openOutputStream(result)),
                compression = false
            )
            return@newInstance when (zipResult) {
                TaskState.Completed -> getString(R.string.export_success)
                TaskState.Cancelled, TaskState.Failed -> getString(R.string.export_failed)
            }
        }.show(parentFragmentManager, ProgressDialogFragment.TAG)
    }
}
