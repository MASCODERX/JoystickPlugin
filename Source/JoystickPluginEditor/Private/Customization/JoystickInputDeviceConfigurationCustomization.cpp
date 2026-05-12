// JoystickPlugin is licensed under the MIT License.
// Copyright Jayden Maalouf 2026. All Rights Reserved.

#include "Customization/JoystickInputDeviceConfigurationCustomization.h"

#if WITH_EDITOR

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "JoystickInputSettings.h"
#include "Managers/JoystickProfileManager.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "JoystickInputDeviceConfigurationCustomization"

void FJoystickInputDeviceConfigurationCustomization::CustomizeHeader(const TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const bool bIsDeviceConfigurationArrayItem = IsDeviceConfigurationArrayItem(StructPropertyHandle);

	HeaderRow
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this, StructPropertyHandle]()
				{
					return GetConnectedDeviceDisplayName(StructPropertyHandle);
				})
				.ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f, 1.0f)))
				.TextStyle(FAppStyle::Get(), "SmallText")
				.Visibility_Lambda([this, StructPropertyHandle]()
				{
					return GetConnectedDeviceDisplayNameVisibility(StructPropertyHandle);
				})
			]
		]
		.ValueContent()
		.MinDesiredWidth(350.0f);

	if (bIsDeviceConfigurationArrayItem)
	{
		HeaderRow.ExtensionContent()
		[
			SNew(SButton)
			.Text(LOCTEXT("CreateProfileButton", "Convert to Profile"))
			.ToolTipText(LOCTEXT("ConvertToProfileTooltip", "Write this device configuration to a deployable profile in the plugin Profiles directory."))
			.OnClicked_Lambda([this, StructPropertyHandle]()
			{
				return ConvertToProfile(StructPropertyHandle);
			})
		];
	}
}

void FJoystickInputDeviceConfigurationCustomization::CustomizeChildren(const TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 ChildCount = 0;
	StructPropertyHandle->GetNumChildren(ChildCount);
	for (uint32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		if (TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex))
		{
			StructBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

bool FJoystickInputDeviceConfigurationCustomization::IsDeviceConfigurationArrayItem(const TSharedRef<IPropertyHandle>& StructPropertyHandle) const
{
	TSharedPtr<IPropertyHandle> ParentHandle = StructPropertyHandle->GetParentHandle();
	while (ParentHandle.IsValid())
	{
		if (const FProperty* ParentProperty = ParentHandle->GetProperty())
		{
			if (ParentProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UJoystickInputSettings, DeviceConfigurations))
			{
				return true;
			}
		}

		ParentHandle = ParentHandle->GetParentHandle();
	}

	return false;
}

bool FJoystickInputDeviceConfigurationCustomization::IsProfileOrDeviceConfigurationArrayItem(const TSharedRef<IPropertyHandle>& StructPropertyHandle) const
{
	TSharedPtr<IPropertyHandle> ParentHandle = StructPropertyHandle->GetParentHandle();
	while (ParentHandle.IsValid())
	{
		if (const FProperty* ParentProperty = ParentHandle->GetProperty())
		{
			const FName ParentPropertyName = ParentProperty->GetFName();
			if (ParentPropertyName == GET_MEMBER_NAME_CHECKED(UJoystickInputSettings, DeviceConfigurations) ||
				ParentPropertyName == GET_MEMBER_NAME_CHECKED(UJoystickInputSettings, ProfileConfigurations))
			{
				return true;
			}
		}

		ParentHandle = ParentHandle->GetParentHandle();
	}

	return false;
}

FText FJoystickInputDeviceConfigurationCustomization::GetConnectedDeviceDisplayName(const TSharedRef<IPropertyHandle>& StructPropertyHandle) const
{
	if (!IsProfileOrDeviceConfigurationArrayItem(StructPropertyHandle))
	{
		return FText::GetEmpty();
	}

	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);
	if (RawData.Num() != 1 || RawData[0] == nullptr)
	{
		return FText::GetEmpty();
	}

	const FJoystickInputDeviceConfiguration& DeviceConfiguration = *static_cast<FJoystickInputDeviceConfiguration*>(RawData[0]);
	const UJoystickInputSettings* InputSettings = GetDefault<UJoystickInputSettings>();
	if (!InputSettings)
	{
		return FText::GetEmpty();
	}

	return FText::FromString(InputSettings->GetMatchingConnectedDeviceProfileDisplayName(DeviceConfiguration));
}

EVisibility FJoystickInputDeviceConfigurationCustomization::GetConnectedDeviceDisplayNameVisibility(const TSharedRef<IPropertyHandle>& StructPropertyHandle) const
{
	return GetConnectedDeviceDisplayName(StructPropertyHandle).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply FJoystickInputDeviceConfigurationCustomization::ConvertToProfile(const TSharedRef<IPropertyHandle>& StructPropertyHandle) const
{
	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);
	if (RawData.Num() != 1 || RawData[0] == nullptr)
	{
		return FReply::Handled();
	}

	const FJoystickInputDeviceConfiguration& DeviceConfiguration = *static_cast<FJoystickInputDeviceConfiguration*>(RawData[0]);

	UJoystickProfileManager* ProfileManager = GetMutableDefault<UJoystickProfileManager>();
	if (!ProfileManager)
	{
		return FReply::Handled();
	}

	TSharedPtr<SEditableTextBox> ProfileNameTextBox;
	bool bAccepted = false;

	const FText InitialProfileName = DeviceConfiguration.OverrideDeviceName && !DeviceConfiguration.DeviceName.IsEmpty()
		                                 ? FText::FromString(DeviceConfiguration.DeviceName)
		                                 : FText::GetEmpty();

	const TSharedRef<SWindow> DialogWindow = SNew(SWindow)
		.Title(LOCTEXT("CreateProfileDialogTitle", "Convert to Joystick Profile"))
		.ClientSize(FVector2D(460.0f, 172.0f))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	DialogWindow->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(18.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("GraphEditor.PadEvent_16x"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateProfileHeading", "Convert to Joystick Profile"))
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
						.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateProfileDescription", "Save this device configuration as a reusable profile and remove it from project settings."))
						.TextStyle(FAppStyle::Get(), "SmallText")
						.AutoWrapText(true)
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 18.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ProfileNameLabel", "Profile Name"))
				.TextStyle(FAppStyle::Get(), "NormalText")
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ProfileNameTextBox, SEditableTextBox)
				.Text(InitialProfileName)
				.HintText(LOCTEXT("ProfileNameHint", "Enter a profile name"))
				.SelectAllTextWhenFocused(true)
				.OnTextCommitted_Lambda([&bAccepted, DialogWindow, &ProfileNameTextBox](const FText&, ETextCommit::Type CommitType)
				{
					const FString TrimmedProfileName = ProfileNameTextBox.IsValid() ? ProfileNameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
					if (CommitType == ETextCommit::OnEnter && !TrimmedProfileName.IsEmpty())
					{
						bAccepted = true;
						DialogWindow->RequestDestroyWindow();
					}
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 14.0f, 0.0f, 0.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SBox)
					.MinDesiredWidth(96.0f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("CreateProfile", "Convert"))
						.IsEnabled_Lambda([&ProfileNameTextBox]
						{
							const FString TrimmedProfileName = ProfileNameTextBox.IsValid() ? ProfileNameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
							return !TrimmedProfileName.IsEmpty();
						})
						.OnClicked_Lambda([&bAccepted, DialogWindow]
						{
							bAccepted = true;
							DialogWindow->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SBox)
					.MinDesiredWidth(96.0f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("CancelProfileCreation", "Cancel"))
						.OnClicked_Lambda([DialogWindow]
						{
							DialogWindow->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]
		]
	);

	DialogWindow->SetWidgetToFocusOnActivate(ProfileNameTextBox);
	FSlateApplication::Get().AddModalWindow(DialogWindow, FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr));

	if (!bAccepted || !ProfileNameTextBox.IsValid())
	{
		return FReply::Handled();
	}

	const FString ProfileName = ProfileNameTextBox->GetText().ToString().TrimStartAndEnd();
	if (ProfileName.IsEmpty())
	{
		return FReply::Handled();
	}

	if (ProfileManager->CreateJoystickProfile(ProfileName, DeviceConfiguration))
	{
		UJoystickInputSettings* InputSettings = GetMutableDefault<UJoystickInputSettings>();
		InputSettings->RemoveDeviceConfiguration(StructPropertyHandle->GetIndexInArray());
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

#endif
